#pragma once

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpServer.h"
#include "muduo/base/Timestamp.h"
#include <functional>
#include <vector>
#include <memory>

using namespace muduo::net;

/**
 * 明文连接处理类
 * 功能：处理Nginx转发的明文数据，无需SSL加密/解密
 */
class PlaintextConnection 
{
private:
    TcpConnectionPtr m_conn;         // muduo原生TCP连接
    std::vector<unsigned char> m_buf; // 接收数据缓冲区
    //size_t m_recvSize;               // 当前缓冲区已接收数据大小

public:	
    PlaintextConnection(const TcpConnectionPtr& conn)
        : m_conn(conn)
    {
        m_buf.resize(1024 * 10); // 初始化缓冲区
    }

    ~PlaintextConnection()
    {
    LOG_INFO << "PlaintextConnection destroyed, ref count: " << m_conn.use_count();
     // 主动关闭连接，确保资源正确释放
        if (m_conn->connected()) {
            m_conn->shutdown();
        }
        else{
            LOG_INFO << "PlaintextConnection destroyed";
        }
    }

    TcpConnectionPtr get_conn() const { return m_conn; }

    // 设置回调函数
    void set_connected_callback(std::function<void()> fun) { m_connectedCallback = fun; }
    void set_receive_callback(std::function<int(PlaintextConnection*, const char*, size_t)> fun) { m_receiveCallback = fun; }
    void set_close_callback(std::function<void()> fun) { m_closeCallback = fun; }

    // TCP连接建立回调
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            LOG_INFO << "Plaintext connection established";
            if (m_connectedCallback) m_connectedCallback();
        }
        else
        {
            LOG_WARN << "Plaintext connection closed";
            if (m_closeCallback) m_closeCallback();
        }
    }
    
    // TCP数据接收回调（直接处理明文）
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, muduo::Timestamp time)
    {
        LOG_INFO << "Receive plaintext data, size: " << buf->readableBytes();
        //processData(buf->peek(), buf->readableBytes());
         // 直接传递Buffer中的数据给回调函数，避免手动管理缓冲区
         if (m_receiveCallback) {
            m_receiveCallback(this, buf->peek(), buf->readableBytes());
        }
        
        buf->retrieveAll(); // 清空缓冲区
    }

    // // 处理明文数据
    // void processData(const char* data, size_t len)
    // {
    //     // 确保缓冲区足够大
    //     if (m_buf.size() < m_recvSize + len) {
    //         m_buf.resize(m_recvSize + len + 1024);
    //     }
    //     // 复制数据
    //     memcpy(m_buf.data() + m_recvSize, data, len);
    //     m_recvSize += len;
        
    //     // 调用接收回调
    //     if (m_receiveCallback) {
    //         m_receiveCallback(this, reinterpret_cast<const char*>(m_buf.data()), m_recvSize);
    //     }
        
    //     // 处理完毕后清空缓冲区
    //     m_recvSize = 0;
    // }

    // 在PlaintextConnection中添加连接状态检查
    bool isConnected() const
    {
        return m_conn->connected();
    }

    // 发送明文数据
    void sendData(const char* data, size_t size)
    {
        if (isConnected()) {
            m_conn->send(data, size);
            LOG_INFO << "Sent plaintext data, size: " << size;
        } else {
            LOG_WARN << "Connection is closed, cannot send data";
        }
    }

private:
    std::function<void()> m_connectedCallback;   // 连接建立回调
    std::function<int(PlaintextConnection*, const char*, size_t)> m_receiveCallback; // 数据接收回调
    std::function<void()> m_closeCallback;       // 连接关闭回调
};

// 明文连接智能指针
typedef std::shared_ptr<PlaintextConnection> PlaintextConnectionPtr;