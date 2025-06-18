#ifndef REDIS_H
#define REDIS_H


#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
#include <string>
using namespace std;

// redis 作为集群服务器通信的基于发布-订阅信息队列时,会遇到两个难搞的bug问题，参考
// https://blog.csdn.net/QIANGWEIYUSN/articel/details/97895611
/*
    Redis 客户端封装类
    提供发布/订阅功能和用户状态管理功能
    使用三个独立的 Redis 上下文分别处理发布、订阅和数据操作

*/

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

    //获取用户在线状态
    string get_state(int id);

    //设置用户在线状态
    bool set_state(int id,string state);

    //添加用户到Redis(通常用于初始化用户状态)
    bool add_user(int id);

    //从Redis删除用户(通常用户清除用户状态)
    bool del_user(int id);

private:

    //hiredis同步上下文对象，负责publish信息
    redisContext * _publish_context;

    //hiredis同步上下文对象，负责subscribe信息
    redisContext *_subcribe_context;

    //hiredis同步上下文对象，负责设置和获取用户在线状态
    redisContext *_data_context;

    //回调操作，收到订阅信息，给service 层上报
    function<void(int ,string)> _notify_message_handler;

    string redis_password;


};

#endif