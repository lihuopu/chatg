#include "clientssl.h"

/**
 * 初始化SSL连接并执行握手
 * param cert_path 证书文件路径
 * param key_path 私钥文件路径
 * param mode SSL模式（客户端或服务器）
 * param fd 已连接的套接字文件描述符
 * return 成功返回SSL对象指针，失败返回NULL
 */
SSL* sync_initialize_ssl(const char* ca_path ,const char* cert_path, const char* key_path, SSL_MODE mode, int fd) {
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    SSL *ssl = NULL;
    bool handshake_success = false;
    
    // 初始化OpenSSL库环境
    // 1. 初始化SSL库
    // 2. 添加所有可用的加密算法
    // 3. 加载错误字符串用于错误处理
    // 4. 清除所有错误状态
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_clear_error();

    // 根据操作模式选择合适的SSL方法
    // 注意：已禁用不安全的SSLv2/SSLv3/TLSv1.0/TLSv1.1
    if (mode == SSL_MODE_SERVER) {
        //method = SSLv23_server_method();
        method = TLS_server_method();// 仅支持TLSv1.2及以上
    } else if (mode == SSL_MODE_CLIENT) {
        //method = SSLv23_client_method();
        method = TLS_client_method();// 仅支持TLSv1.2及以上
    } else {
        // 未知模式，无法继续
        //printf("Not found method");
        fprintf(stderr, "Unsupported SSL mode: %d\n", mode);
        return NULL;
    }

    // 创建SSL上下文，用于管理SSL会话的参数和配置
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        printf("Unable to create SSL context");
        return NULL;
    }

    /*
    // 加载并验证服务器CA证书（用于验证对端证书）——双向认证
    if (ca_path && SSL_CTX_load_verify_locations(ctx, ca_path, NULL) != 1) {
        print_ssl_error("Failed to load CA certificates");
        SSL_CTX_free(ctx);
        return NULL;
    }
    */
    // 配置SSL上下文：加载证书和私钥
    // 注意：证书和私钥必须匹配且格式正确
    if(cert_path&&key_path){
        //加载客户端证书文件和私钥文件(自身)
        if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) <= 0 || 
        SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0 ) {
        printf("Not found certificate or private key");
        SSL_CTX_free(ctx);
        return NULL;  
        }

        //验证证书和私钥是否匹配
        if (!SSL_CTX_check_private_key(ctx)) {
            print_ssl_error("Certificate and private key do not match");
            SSL_CTX_free(ctx);
            return NULL;
        }
    }
    
    // 生产环境应使用SSL_VERIFY_PEER并提供CA证书
    //SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    //配置验证模式 - 双向认证(双向认证)
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    SSL_CTX_set_verify_depth(ctx, 5); // 设置证书链最大深度

    // 创建SSL对象，代表一个SSL连接实例
    ssl = SSL_new(ctx);
    if (!ssl) {
        //fprintf("Failed to create SSL object.");
        print_ssl_error("Failed to create SSL object");
        SSL_CTX_free(ctx);
        return NULL;
    }

    // 将SSL对象与已连接的套接字文件描述符关联
    if (SSL_set_fd(ssl, fd) == 0) {
        //printf("Failed to set fd to SSL object.");
        print_ssl_error("Failed to associate socket with SSL object");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return NULL;
    }

    // 执行SSL握手
    // 客户端调用SSL_connect发起握手
    // 服务器端调用SSL_accept响应握手
    int ssl_result = -1;
    if (mode == SSL_MODE_CLIENT) {
        ssl_result = SSL_connect(ssl);
    } else if (mode == SSL_MODE_SERVER) {
        ssl_result = SSL_accept(ssl);
    }

    //检查握手结果
    if (ssl_result <= 0) {
        int ssl_err = SSL_get_error(ssl, ssl_result);
        print_ssl_handshake_error(ssl_err);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return NULL;
    }

    // 握手成功，清理上下文但保留SSL对象
    SSL_CTX_free(ctx); // 上下文在握手后不再需要，可以释放
    return ssl;
    
}

/**
 * 打印通用SSL错误信息
 */
void print_ssl_error(const char* message) {
    fprintf(stderr, "SSL error: %s\n", message);
    unsigned long err_code = ERR_get_error();
    if (err_code != 0) {
        char err_msg[256] = {0};
        //将错误码转换为可读字符串
        ERR_error_string_n(err_code, err_msg, sizeof(err_msg));
        fprintf(stderr, "OpenSSL error detail: %s\n", err_msg);
    }
}

/**
 * 打印SSL握手错误详情
 */
void print_ssl_handshake_error(int ssl_err) {
    fprintf(stderr, "===============SSL handshake failed. Error code: %d========!\n", ssl_err);
    
    const char *err_str;
    switch (ssl_err) {
        case SSL_ERROR_NONE:
            err_str = "No error";
            break;
        case SSL_ERROR_SSL:
            err_str = "Error in the SSL protocol";
            break;
        case SSL_ERROR_WANT_READ:
            err_str = "Need to read more data";
            break;
        case SSL_ERROR_WANT_WRITE:
            err_str = "Need to write more data";
            break;
        case SSL_ERROR_WANT_X509_LOOKUP:
            err_str = "X.509 lookup operation pending";
            break;
        case SSL_ERROR_SYSCALL:
            err_str = "System call error";
            break;
        case SSL_ERROR_ZERO_RETURN:
            err_str = "Connection closed cleanly";
            break;
        case SSL_ERROR_WANT_CONNECT:
            err_str = "Connect operation pending";
            break;
        case SSL_ERROR_WANT_ACCEPT:
            err_str = "Accept operation pending";
            break;
        default:
            err_str = "Unknown error";
            break;
    }
    
    fprintf(stderr, "Handshake error: %s\n", err_str);
    print_ssl_error("Handshake failed");
}