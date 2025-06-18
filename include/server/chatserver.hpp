#ifndef CHATSERVER_H
#define CHATSERVER_H

#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
//#include "SSLConnection.h"
#include "PlaintextConnection.h"
#include <unordered_map>
#include <mutex>
using namespace muduo;
using namespace muduo::net;


//聊天服务器的主类
class ChatServer{
    public:
    //初始化聊天服务器对象
        ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg);
    
    //启动服务
        void start();
        
    private:
    //上报链接相关信息的回调函数
    void onConnection(const TcpConnectionPtr&);

    void handleConnectionClose(const TcpConnectionPtr& conn);

    //上报读写事件相关信息的回调函数
    void onMessage(const TcpConnectionPtr&,
        Buffer*,
        Timestamp);
    /**
     * 处理SSL解密后的数据
     * param ssl_conn SSL连接对象指针
     * param data 解密后的原始数据
     * param datalen 数据长度
     * return 处理结果状态码
     */
    //using MsgHandler = std::function<void(const PlaintextConnectionPtr &conn, json &js, Timestamp)>;
    int handledata(PlaintextConnection* ssl_conn, unsigned char* data, size_t datalen);
    TcpServer _server;//组合的muduo库，实现服务器功能的类对象
    EventLoop *_loop;//指向事件循化对象的指针
   
};

#endif