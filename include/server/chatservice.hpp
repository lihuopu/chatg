# ifndef CHATSERVICE_H

# define CHATSERVICE_H

#include<muduo/net/TcpConnection.h>
#include <unordered_map>
#include<functional>
//#include <memory>
#include "redis.hpp"
#include "groupmodel.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "json.hpp"
#include "friendmodel.hpp"
//#include "SSLConnection.h"
#include "PlaintextConnection.h"
#include <mutex>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

//表示处理消息事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js ,Timestamp )>;
//聊天服务器业务类
class ChatService{
    public:
    //获取单例对象的接口函数
    static ChatService* instance();

    //处理登录业务
    void login(const TcpConnectionPtr &conn, json &js ,Timestamp);
    //处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js ,Timestamp);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js ,Timestamp);
    //添加好友业务
    void addFriend (const TcpConnectionPtr &conn, json &js ,Timestamp);
    //创建群组业务
    void createGroup (const TcpConnectionPtr &conn, json &js ,Timestamp);
    //加入群组业务
    void addGroup (const TcpConnectionPtr &conn, json &js ,Timestamp);
    //群组聊天业务
    void groupChat (const TcpConnectionPtr &conn, json &js ,Timestamp);
    //处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js ,Timestamp);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr & conn);
    //服务器异常，业务重置方法
    void reset();
    //获取信息对应的处理器
    MsgHandler getHandler(int msgid);
    //从redis信息队列中获取订阅的信息
    void handleRedisSubscribeMessage(int,string);
    private:

    //私有构造函数，确保 ChatService 类只能通过 instance() 方法获取单例对象。
    ChatService();

    //存储消息id和其对应的业务处理方法
    unordered_map<int ,MsgHandler> _msgHandlerMap;

    //存储在线用户的通信连接
    unordered_map<int ,TcpConnectionPtr> _userConnMap;

    //数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    //定义互斥锁，保证_userConnMap的线程安全
    mutex _connMutex;

    //redis操作对象
    Redis _redis;
};

#endif