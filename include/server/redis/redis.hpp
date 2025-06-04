#ifndef REDIS_H
#define REDIS_H


#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
using namespace std;

// redis 作为集群服务器通信的基于发布-订阅信息队列时,会遇到两个难搞的bug问题，参考
// https://blog.csdn.net/QIANGWEIYUSN/articel/details/97895611

class Redis{
public:
    Redis();
    ~Redis();

    //连接redis服务器
    bool connect();

    //向redis指定的通道channel发布信息
    bool publish(int channel ,string messages);

    //向redis指定的通道subscribe订阅信息
    bool subscribe(int channel);

    //向reids指定的通道unsubscirbe取消订阅信息
    bool unsubscribe(int channel);

    //在独立线程中接收订阅通道中的信息
    void observer_channel_message();

    //初始化向业务层上报通道信息的回调对象
    void init_notify_handler(function<void(int ,string)> fn);

private:

    //hiredis同步上下文对象，负责publish信息
    redisContext * _publish_context;

    //hiredis同步上下文对象，负责subscribe信息
    redisContext *_subcribe_context;

    //回调操作，收到订阅信息，给service 层上报
    function<void(int ,string)> _notify_message_handler;


};

#endif