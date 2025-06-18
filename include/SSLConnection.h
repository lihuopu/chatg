#pragma once
#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpServer.h"
#include "muduo/base/Timestamp.h"
#include <functional>
#include <vector>
#include <memory>
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "cert.h"
using namespace muduo::net;

/**
 * SSL 操作类型枚举
 * 用途：区分内存 BIO 的读写方向
 */
enum OVERLAPPED_TYPE
{
	RECV = 0,
	SEND = 1
};

/**
 * SSL 连接角色枚举
 * 用途：区分客户端和服务器模式
 */
enum SSL_TYPE
{
	CLIENT = 0,
	SERVER = 1
};

/**
 * SSL 安全连接封装类
 * 功能：
 * - 封装 OpenSSL 库，提供安全的加密通信
 * - 基于 muduo 网络库的 TcpConnection 实现
 * - 支持双向认证和数据加密传输
 */
class SSLConnection 
{
private:
	TcpConnectionPtr _conn;// muduo 原生 TCP 连接对象


	SSL_CTX *m_SslCtx; //SSL 上下文
	SSL *m_Ssl;     // SSL 连接对象
	BIO *m_Bio[2]; // 内存 BIO 对象（用于读写缓冲）

	bool m_Handshaked;   // SSL 握手完成标志

    //数据缓冲区
	std::vector<unsigned char> m_EncryptedSendData;// 加密发送数据缓冲区
	std::vector<unsigned char> m_DecryptedRecvData;// 解密接收数据缓冲区
	int m_SendSize;    //发送数据大小

    //统计信息
	unsigned long long  m_BytesSizeRecieved;// 接收数据总大小
	unsigned long long  m_TotalRecived;// 当前处理的总接收数据
	unsigned long long  m_CurrRecived;// 当前缓冲区已接收数据

public:	
	SSLConnection(const TcpConnectionPtr& conn)
	{   
        //初始化缓冲区和状态
		m_EncryptedSendData.resize(1024 * 10);
		m_DecryptedRecvData.resize(1024 * 10);
		m_CurrRecived = 0;
		m_BytesSizeRecieved = 0;
		m_TotalRecived = 0;
		m_Handshaked = false;

		_conn = conn;
	}

	~SSLConnection()
	{
		printf("~SSL_Helper()\n");
		if (m_Ssl)
		{
			SSL_free(m_Ssl);
		}
		if (m_SslCtx)
		{
			SSL_CTX_free(m_SslCtx);
		}

	}

    /**
     * 设置 SSL 连接类型
     * 参数：
     *   - type: SSL 类型（客户端或服务器）
     */
	void set_type(SSL_TYPE type)
	{
		m_type = type;
	}
	//获取原始TCP连接
	TcpConnectionPtr get_conn() { return _conn; }

    //回调函数设置
	void set_connected_callback(std::function<void()> fun) { m_SSL_connected_callback = fun; }
	void set_receive_callback(std::function<int(SSLConnection*, unsigned char*, size_t)> fun) { m_SSL_receive_callback = fun; }

	/**
     * TCP 连接建立回调
     * 根据连接类型执行 SSL 握手（服务器或客户端）
     */
	void onConnection(const TcpConnectionPtr& conn)
	{
		if (conn->connected())
		{
			if(m_type == SERVER)
				do_ssl_accept();// 服务器模式：等待客户端连接
			else 
				do_ssl_connect();// 客户端模式：主动连接服务器
		}
		else
		{
			LOG_WARN << "connect closed";
		}
	}
    
    //TCP数据接收回调
	//调用顺序：onMessage->SSLProcessingRecv->SSLReceiveData->m_SSL_receive_callback
	void onMessage(const TcpConnectionPtr& conn, Buffer* buf, muduo::Timestamp time)
	{
		printf("receive data, size:%d \n", buf->readableBytes());

		auto datalen = buf->readableBytes();
		m_BytesSizeRecieved += datalen;
		SSLProcessingRecv(buf->peek(), datalen);//处理收到的加密数据
		buf->retrieveAll();//清空缓冲区
	}

	//SSL 数据接收处理 解密数据后调用用户回调 
	void SSLReceiveData()
	{
		printf("m_CurrRecived:%d ", m_CurrRecived);
		printf("m_TotalRecived:%d\n ", m_TotalRecived);

		if (m_SSL_receive_callback)
			m_SSL_receive_callback(this, m_DecryptedRecvData.data(), m_DecryptedRecvData.size());
		
		m_CurrRecived = 0;
		m_TotalRecived = 0;
		// m_DecryptedRecvData.clear();
	}
	
	//SSL连接建立回调  通知用户SSL 握手成功
	void SSLConnected()
	{
		if (m_SSL_connected_callback)
			m_SSL_connected_callback();

	}

private:
    //回调函数
	std::function<void()> m_SSL_connected_callback;// SSL 连接建立回调
	std::function<int(SSLConnection*, unsigned char*, size_t)> m_SSL_receive_callback;// SSL 数据接收回调
	std::function<void()> m_SSL_closed_callback;// SSL 连接关闭回调
	SSL_TYPE m_type; // SSL 连接类型

    // 关闭会话  断开TCP连接并清理资源
	void close_session()
	{
		_conn->forceClose();
		printf("close_session()\n");
	}

public:
    /**
     * 发送 SSL 加密数据
     * 参数：
     *   - data: 明文数据
     *   - size: 数据大小
     */
	void SSLSendData(const char* data, size_t size)
	{
		int	ret = SSL_write(m_Ssl, data, size);// OpenSSL 加密写入
		int	ssl_error = SSL_get_error(m_Ssl, ret);

		if (IsSSLError(ssl_error))
			close_session();

		SSLProcessingSend(); //处理发送缓冲区
	}

	//服务器模式：初始化 SSL 握手
	int do_ssl_accept()
	{
		init_ssl(); // 初始化 OpenSSL 库
		CreateServerSSLContext();// 创建服务器 SSL 上下文
		SSL_set_accept_state(m_Ssl);// 设置为服务器接受状态
		SSLProcessingAccept();// 处理握手过程
		return 1;
	}
	
	//客户端模式：初始化 SSL 握手
	int do_ssl_connect()
	{
		init_ssl();// 初始化 OpenSSL 库
		CreateClientSSLContext();// 创建客户端 SSL 上下文
		SSL_set_connect_state(m_Ssl);// 设置为客户端连接状态
		SSLProcessingConnect();// 处理握手过程
		return 1;
	}
	
    //处理服务器 SSL 握手
	void SSLProcessingAccept()
	{
		int ret;
		int ssl_error;

		int dwBytesSizeRecieved = 0;

        // 循环读取 SSL 数据，直到没有更多数据
		do
		{
			ret = SSL_read(m_Ssl, m_DecryptedRecvData.data(), m_DecryptedRecvData.size());
			ssl_error = SSL_get_error(m_Ssl, ret);

			if (IsSSLError(ssl_error))
				close_session();

			if (ret > 0)
				dwBytesSizeRecieved += ret;
		} while (ret > 0);

          // 检查握手是否完成
		if (SSL_is_init_finished(m_Ssl))
		{
			m_Handshaked = true;
			SSLReceiveData();// 接收并处理数据
		}

		SSLProcessingSend();// 处理发送缓冲区
	}

    //处理客户端 SSL 握手
	void SSLProcessingConnect()
	{
		int ret;
		int ssl_error;

		int bytesSizeRecieved = 0;
        //循环读取 SSL 数据，直到没有更多数据
		do
		{
			ret = SSL_read(m_Ssl, m_DecryptedRecvData.data(), m_DecryptedRecvData.size());
			ssl_error = SSL_get_error(m_Ssl, ret);

			if (IsSSLError(ssl_error))
				close_session();

			if (ret > 0)
				bytesSizeRecieved += ret;

		} while (ret > 0);

        //检查握手是否完成
		if (SSL_is_init_finished(m_Ssl))
		{
			m_Handshaked = true;
			SSLReceiveData();//接收并处理数据
		}

		SSLProcessingSend();//处理发送缓冲区
	}

    //处理 SSL 发送缓冲区  将加密后的数据通过 TCP 发送
	void SSLProcessingSend()
	{
		int ret;
		int ssl_error;

        // 循环读取 BIO 缓冲区，直到没有更多数据
		while (BIO_pending(m_Bio[SEND]))
		{
			ret = BIO_read(m_Bio[SEND], m_EncryptedSendData.data(), m_EncryptedSendData.size());

			if (ret > 0)
			{
				_conn->send(reinterpret_cast<char*>(m_EncryptedSendData.data()), ret);
			}
			else
			{
				ssl_error = SSL_get_error(m_Ssl, ret);

				if (IsSSLError(ssl_error))
					close_session();
			}
		}
	}

    /**
     * 处理 SSL 接收数据
     *   - RecvBuffer: 接收到的加密数据
     *   - BytesSizeRecieved: 数据大小
     */
	void SSLProcessingRecv(const char* RecvBuffer, size_t BytesSizeRecieved)
	{
		int ret;
		int ssl_error;

        //// 将加密数据写入接收 BIO
		if (m_BytesSizeRecieved > 0)
		{
			ret = BIO_write(m_Bio[RECV], RecvBuffer, BytesSizeRecieved);

			if (ret > 0)
			{
				int intRet = ret;
				if (intRet > m_BytesSizeRecieved)
					close_session();

				m_BytesSizeRecieved -= intRet;
			}
			else
			{
				ssl_error = SSL_get_error(m_Ssl, ret);
				if (IsSSLError(ssl_error))
					close_session();
			}
		}

        //循环读取并解密数据
		do
		{
			assert(m_DecryptedRecvData.size() - (int)m_CurrRecived > 0);
			ret = SSL_read(m_Ssl, m_DecryptedRecvData.data() + m_CurrRecived, m_DecryptedRecvData.size() - m_CurrRecived);

			if (ret > 0)
			{
				m_CurrRecived += ret;
				m_TotalRecived += ret;

                //握手完成后处理解密数据
				if (m_Handshaked)
				{
					SSLReceiveData();
				}
			}
			else
			{
				ssl_error = SSL_get_error(m_Ssl, ret);

				if (IsSSLError(ssl_error)){
					unsigned long err = ERR_get_error();
    				char *err_str = ERR_error_string(err, NULL);
					LOG_ERROR << "SSL_read error: " << err_str;
					close_session();
				}
					
			}
		} while (ret > 0);

        //检查握手是否完成
		if (!m_Handshaked)
		{
			if (SSL_is_init_finished(m_Ssl))
			{
				m_Handshaked = true;
				SSLConnected();//通知握手完成
			}
		}

		SSLProcessingSend(); //处理发送缓冲区
	}

    //初始化OpenSSL 库
	void init_ssl()
	{
		SSL_load_error_strings();
		SSL_library_init();

		OpenSSL_add_all_algorithms();
		ERR_load_BIO_strings();
	}

    //创建客户端 SSL 上下文
	void CreateClientSSLContext()
	{
		m_SslCtx = SSL_CTX_new(SSLv23_method());//创建支持 SSLv2/3 和 TLS 的上下文
		SSL_CTX_set_verify(m_SslCtx, SSL_VERIFY_NONE, nullptr);// 禁用证书验证（不安全）

		m_Ssl = SSL_new(m_SslCtx);//创建SSL对象

        // 创建内存 BIO 用于数据读写
		m_Bio[SEND] = BIO_new(BIO_s_mem());
		m_Bio[RECV] = BIO_new(BIO_s_mem());
		SSL_set_bio(m_Ssl, m_Bio[RECV], m_Bio[SEND]);// 关联 BIO 到 SSL 对象
	}

    //创建服务器SSL上下文
	void CreateServerSSLContext()
	{
		m_SslCtx = SSL_CTX_new(SSLv23_server_method());// 创建服务器模式的上下文
		SSL_CTX_set_verify(m_SslCtx, SSL_VERIFY_NONE, nullptr);// 禁用客户端证书验证（单向认证）


		SetSSLCertificate();// 设置服务器证书
		m_Ssl = SSL_new(m_SslCtx);// 创建 SSL 对象

        // 创建内存 BIO 用于数据读写
		m_Bio[SEND] = BIO_new(BIO_s_mem());
		m_Bio[RECV] = BIO_new(BIO_s_mem());
		SSL_set_bio(m_Ssl, m_Bio[RECV], m_Bio[SEND]);// 关联 BIO 到 SSL 对象

	}

    //设置服务器证书和私钥
	void SetSSLCertificate()
	{
		// int length = strlen(server_cert_key_pem);
		// BIO *bio_cert = BIO_new_mem_buf((void*)server_cert_key_pem, length);
		// X509 *cert = PEM_read_bio_X509(bio_cert, nullptr, nullptr, nullptr);
		// EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio_cert, 0, 0, 0);
        //从文件加载证书和私钥
        /*
            certificate.pem — server.cert.pem 服务器证书
            private_key.pem — server.key.pem 服务器私钥

        */
		const char* cert_path = "/home/ca-certificates/certs/server.cert.pem";
		const char* key_path = "/home/ca-certificates/private/server.key.pem";


		int ret = SSL_CTX_use_certificate_file(m_SslCtx, cert_path, SSL_FILETYPE_PEM);

		if (ret != 1){
			LOG_ERROR << "Failed to load certificate, error: " << ERR_error_string(ERR_get_error(), NULL);
			close_session();
		}

		ret = SSL_CTX_use_PrivateKey_file(m_SslCtx, key_path, SSL_FILETYPE_PEM);

		if (ret != 1){
			LOG_ERROR << "Failed to load key, error: " << ERR_error_string(ERR_get_error(), NULL);
			close_session();
		}
			

		// X509_free(cert);
		// EVP_PKEY_free(pkey);
		// BIO_free(bio_cert);
	}


    /**
     * 判断 SSL 错误类型
     *   - ssl_error: SSL 错误码
     * 返回：
     *   - true: 需要关闭连接的错误
     *   - false: 可忽略的临时错误
     */
	bool IsSSLError(int ssl_error)
	{
		switch (ssl_error)
		{
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_SYSCALL: // 新增：允许系统调用错误重试
		case SSL_ERROR_ZERO_RETURN: // 新增：允许握手过程中的正常返回

			return false;

		default: return true;
		}

	}

};
//SSL连接 智能指针类型定义
typedef std::shared_ptr<SSLConnection> SSLConnectionPtr;

