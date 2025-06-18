#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis()
    :_publish_context(nullptr),_subcribe_context(nullptr),redis_password("kang0904")
{

}

Redis::~Redis(){
    if(_publish_context != nullptr){
        redisFree(_publish_context);
    }
    if(_subcribe_context != nullptr){
        redisFree(_subcribe_context);
    }
}

bool Redis::connect(){
    //连接本地redis服务器 ——6379
    //负责publish发布信息的上下文连接
    _publish_context = redisConnect("127.0.0.1",6379);
    if(nullptr == _publish_context){
        cerr<<"connect redis failed!"<<endl;
        return false;
    }

    //负责subscribe订阅信息的上下文连接
    _subcribe_context = redisConnect("127.0.0.1",6379);
    if(nullptr == _subcribe_context){
        cerr<<"connect redis failed!"<<endl;
        return false;
    }

    _data_context = redisConnect("127.0.0.1",6379);
    if(nullptr == _data_context){
        cerr << "connect redis failed!" << endl;
        return false;
    }

    //分别对发布上下文，订阅上下文，数据操作上下文进行认证
    if(!redis_password.empty()){
        if (((redisReply *)redisCommand(_publish_context, "AUTH %s", redis_password.c_str()))->type == REDIS_REPLY_ERROR){
            cerr << "wrong redis password!" << endl;
            return false;
        }
        if (((redisReply *)redisCommand(_subcribe_context, "AUTH %s", redis_password.c_str()))->type == REDIS_REPLY_ERROR){
            cerr << "wrong redis password!" << endl;
            return false;
        }
        if (((redisReply *)redisCommand(_data_context, "AUTH %s", redis_password.c_str()))->type == REDIS_REPLY_ERROR){
            cerr << "wrong redis password!" << endl;
            return false;
        }
    }
    //在单独线程中，监听通道上的事件，有信息给业务层进行上报
    //启动线程后，使用detach方法将其与当前线程分离，使其在后台独立运行。
    thread t([&](){
        observer_channel_message();
    });
    t.detach();

    cout<<"connect redis-server success!"<<endl;

    return true;
}

/*
    typedef struct redisReply {
    int type;            响应类型，取值为 REDIS_REPLY_* 系列常量 
    long long integer;   当 type 为 REDIS_REPLY_INTEGER 时，表示整数值 
    double dval;        当 type 为 REDIS_REPLY_DOUBLE 时，表示双精度浮点值 
    size_t len;          字符串长度（用于二进制安全） 
    char *str;           用于存储以下类型的数据：
                           - REDIS_REPLY_ERROR：错误信息
                           - REDIS_REPLY_STRING：字符串值
                           - REDIS_REPLY_VERB：带类型的字符串（需配合 vtype 字段）
                           - REDIS_REPLY_DOUBLE：字符串形式的浮点数（辅助 dval）
                           - REDIS_REPLY_BIGNUM：大整数字符串表示 
    char vtype[4];       用于 REDIS_REPLY_VERB 类型，表示 3 字符的内容类型（如 "txt"、"bin" 等），以空字符结尾 
    size_t elements;     当 type 为 REDIS_REPLY_ARRAY 时，表示数组元素个数 
    struct redisReply **element; 当 type 为 REDIS_REPLY_ARRAY 时，指向子元素的指针数组 
    }

*/

//获取指定用户的状态信息
string Redis::get_state(int id){
    redisReply *reply = (redisReply *)redisCommand(_data_context, "GET %d", id);
    if(reply->len == 0){
        cout << "user" << id << " is not exist in redis" << endl;
        return "null";
    }
    std::string value(reply->str); // char* -> c++ string
    //释放Redis回复对象，避免内存泄漏
    freeReplyObject(reply);
    return value;
}

//设置指定用户的在线状态
bool Redis::set_state(int id, string state){
    redisReply *reply = (redisReply *)redisCommand(_data_context, "SET %d %s", id, state.c_str());
    if (nullptr == reply)
    {
        cerr << "set command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

//添加用户并设置默认在线状态为"online"
bool Redis::add_user(int id){
    redisReply *reply = (redisReply *)redisCommand(_data_context, "SET %d online", id);
    if (nullptr == reply)
    {
        cerr << "set command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

//删除指定用户的状态数据
bool Redis::del_user(int id){
    redisReply *reply = (redisReply *)redisCommand(_data_context, "DEL %d", id);
    if (nullptr == reply)
    {
        cerr << "set command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}



//向redis指定的通道channel发布信息
bool Redis::publish(int channel ,string message){
    redisReply * reply = (redisReply*)redisCommand(_publish_context,
        "PUBLISH %d %s",channel,message.c_str());
    if(nullptr == reply){
        cerr<< "publish command failed!"<<endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

//向redis指定的通道subscribe订阅消息
//传入参数为用户id
bool Redis::subscribe(int channel){
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    if(REDIS_ERR == redisAppendCommand(this->_subcribe_context,"SUBSCRIBE %d",channel)){
        cerr<< "subscribe command failed!"<<endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while(!done){
        if(REDIS_ERR == redisBufferWrite(this->_subcribe_context,&done)){
            cerr<<"subscribe command failed!"<<endl;
            return false;
        }
    }

    //redisGetReply

    return true;
}

//向redis指定的通道unsubscribe取消订阅信息
bool Redis::unsubscribe(int channel){
    if(REDIS_ERR == redisAppendCommand(this->_subcribe_context,"UNSUBSCRIBE %d",channel)){
        cerr<<"unsubscribe command failed!"<<endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done 置为1)
    int done = 0;
    while(!done){
        if(REDIS_ERR == redisBufferWrite(this->_subcribe_context,&done)){
            cerr <<"unsubscribe command failed!"<<endl;
            return false;
        }
    }
    return true;
}

//在独立线程中接收订阅通道信息
void Redis::observer_channel_message(){
    //redisReply 是 hiredis 库中 Redis 服务器响应的结构体，后续用来存储从 Redis 服务器接收到的消息。
    redisReply * reply = nullptr;
    //REDIS_OK 是 hiredis 库中定义的常量，表示操作成功。
    while(REDIS_OK == redisGetReply(this->_subcribe_context,(void**)&reply)){
        //订阅收到的信息是一个带三元素的数组
        if(reply != nullptr&&reply->element[2] != nullptr && reply->element[2]->str != nullptr){
            //给业务层上报通道上发生的信息
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }

        //释放对象内存
        freeReplyObject(reply);
    }

    cerr<<">>>>>>>>>> observer_channel_message quit<<<<<<<<<"<<endl;
}

    //回调函数
void Redis::init_notify_handler(function<void(int,string)> fn){
    this->_notify_message_handler = fn;
}