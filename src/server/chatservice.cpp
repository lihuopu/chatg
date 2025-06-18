#include "chatservice.hpp"
#include <muduo/base/Logging.h>
#include <string>
#include <vector>
#include <iostream>
#include "public.hpp"
using namespace std;
using namespace muduo;


// 获取单例对象的接口函数——对应单例模式，一个类只有对象，类内创建，外部调用
ChatService *ChatService::instance()
{
   static ChatService service;
   return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
   // 用户基本业务管理相关事件处理回调注册
   _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
   _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
   _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
   _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
   _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

   // 群组业务管理相关事件处理回调注册
   _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
   _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
   _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

   // 连接redis服务器
   if (_redis.connect())
   {
      // 设置上报信息的回调
      //绑定函数功能为获取redis订阅信息
      _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
   }
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
   // 把Online状态的用户，设置成offline
   _userModel.resetState();

   for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it){
      _redis.del_user(it->first);
  }
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{

   // 记录错误日志，msgid 没有对应的事件处理回调
   auto it = _msgHandlerMap.find(msgid);
   if (it == _msgHandlerMap.end())
   {
      // LOG_ERROR << "msgid:"<<msgid << "can not find handler! ";

      // 返回一个默认处理器，空操作
      return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
      {
         LOG_ERROR << "msgid:" << msgid << "can not find handler! ";
      };
   }
   else
      return _msgHandlerMap[msgid];
}

// 处理登录业务 id pwd
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
   int id = js["id"].get<int>();
   string pwd = js["password"];
   User user = _userModel.query(id);
   if (user.getId() == id && user.getPwd() == pwd)
   {

      if (user.getState() == "online")
      {
         // 该用户已经登录，不允许重复登录
         json response;
         response["msgid"] = LOGIN_MSG_ACK;
         response["errno"] = 2;
         response["errmsg"] = "this account is using ,input another";
         conn->send(response.dump());
         // string response_str = response.dump();
         // conn->sendData(response_str.c_str(), response_str.size());
      }
      else
      {

         // 登录成功，记录用户连接信息
         {
            lock_guard<mutex> lock(_connMutex);
            _userConnMap.insert({id, conn});
         }

         // id用户登录成功后,向redis订阅channel(id)
         _redis.subscribe(id);

         // 登录成功，更新用户状态信息 state offline = >online
         user.setState("online");
         _userModel.updateState(user);

         string value = _redis.get_state(id);
         if (value == "null"){
                _redis.add_user(id);
         }
         else if(value == "offline"){
                _redis.set_state(id,"online");
         }



         json response;
         response["msgid"] = LOGIN_MSG_ACK;
         response["errno"] = 0;
         response["id"] = user.getId();
         response["name"] = user.getName();

         // 查询该用户是否有离线信息

         vector<string> vec = _offlineMsgModel.query(id);
         if (!vec.empty())
         {
            response["offlinemsg"] = vec;
            // 读取该用户的离线信息后，把该用户的所有离线信息删除掉
            _offlineMsgModel.remove(id);
         }
         // 查询该用户的好友信息并返回
         vector<User> userVec = _friendModel.query(id);
         if (!userVec.empty())
         {
            vector<string> vec2;
            for (User &user : userVec)
            {
               json js;
               js["id"] = user.getId();
               js["name"] = user.getName();
               js["state"] = user.getState();
               vec2.push_back(js.dump()); // 注意
            }
            response["friends"] = vec2;
         }

         // conn->send(response.dump());
         // 查询用户的群组信息
         vector<Group> groupuseVec = _groupModel.queryGroups(id);
         if (!groupuseVec.empty())
         {
            // group:[{groupid:[xxx,xxx,xxx]}]
            vector<string> groupV;
            for (Group &group : groupuseVec)
            {
               json grpjson;
               grpjson["id"] = group.getId();
               grpjson["groupname"] = group.getName();
               grpjson["groupdesc"] = group.getDesc();
               vector<string> userV;
               for (GroupUser &user : group.getUsers())
               {
                  json js;
                  js["id"] = user.getId();
                  js["name"] = user.getName();
                  js["state"] = user.getState();
                  js["role"] = user.getRole();
                  userV.push_back(js.dump());
               }
               grpjson["users"] = userV;
               groupV.push_back(grpjson.dump());
            }

            response["groups"] = groupV;
         }
         conn->send(response.dump());
         // string response_str = response.dump();
         // conn->sendData(response_str.c_str(), response_str.size());
         //conn->get_conn()->send(response_str);
      }
   }
   else
   {
      // 该用户不存在，用户存在但是密码错误，登录失败
      json response;
      response["msgid"] = LOGIN_MSG_ACK;
      response["errno"] = 1;
      response["errmsg"] = "id or password is invalid!";

      conn->send(response.dump());
      // string response_str = response.dump();
      // conn->sendData(response_str.c_str(), response_str.size());
   }
}

//处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
   string name = js["name"];
   string pwd = js["password"];

   User user;
   user.setName(name);
   user.setPwd(pwd);
   bool state = _userModel.insert(user);

   if (state)
   {
      // 注册成功
      json response;
      response["msgid"] = REG_MSG_ACK;
      response["errno"] = 0; // 修改
      response["id"] = user.getId();
      conn->send(response.dump());
      //string response_str = response.dump();
      //conn->SSLSendData(response_str.c_str(), response_str.size());
     // conn->get_conn()->send(response_str);
   }
   else
   {
      // 注册失败
      json response;
      response["msgid"] = REG_MSG_ACK;
      response["errno"] = 1;
      // response["errmsg"] = 1;
      response["id"] = user.getId();
      conn->send(response.dump());
      // string response_str = response.dump();
      // conn->SSLSendData(response_str.c_str(), response_str.size());
   }
}
// void ChatService::reg(const PlaintextConnectionPtr &conn, json &js, Timestamp time)
// {
//     string name = js["name"];
//     string pwd = js["password"];

//     User user;
//     user.setName(name);
//     user.setPwd(pwd);
//     bool state = _userModel.insert(user);

//     json response;
//     response["msgid"] = REG_MSG_ACK;
//    //  response["errno"] = state ? 0 : 1;
//    //  response["id"] = user.getId();
//    if (state) {
//          // 注册成功
//          response["errno"] = 0;
//          response["id"] = user.getId();
//          LOG_INFO << "发送注册响应: " << response.dump();
//    } else {
//          // 注册失败，获取具体错误信息
//          response["errno"] = 1;
//          response["id"] = 0;
//          response["errmsg"] = "注册失败，用户名可能已存在";
//          LOG_WARN << "注册失败: " << response.dump();
//    }

//     string response_str = response.dump();

//     //LOG_INFO << "发送注册响应: " << response_str;  // 添加日志确认
    
//     // 统一使用明文发送（移除所有SSLSendData调用）
//     //conn->get_conn()->send(response_str);
//     //conn->sendData(response_str.c_str(), response_str.size());
//       // 发送响应前检查连接状态
//       if (conn->isConnected()) {
//          conn->sendData(response_str.c_str(), response_str.size());
//          LOG_INFO << "Sent plaintext data, size: " << response_str.size();
//      } else {
//          LOG_WARN << "Connection closed, cannot send registration response";
//          return;
//      }
// }

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp)
{
   int userid = js["id"].get<int>();

   {
      lock_guard<mutex> lock(_connMutex);
      auto it = _userConnMap.find(userid);
      if (it != _userConnMap.end())
      {
         _userConnMap.erase(it);
      }
   }

   // 用户注销，相当于就是下线，在redis中取消订阅通道
   _redis.unsubscribe(userid);

   // 更新用户的状态信息

   User user(userid, "", "", "offline");
   _userModel.updateState(user);
   _redis.del_user(userid);
}
// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
   User user;
   {
      lock_guard<mutex> lock(_connMutex);

      for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
      {
         if (it->second == conn)
         {
            // 从用户表删除用户的连接信息
            user.setId(it->first);
            _userConnMap.erase(it);
            break;
         }
      }
   }

   // 用户注销，相当于下线，在redis中取消订阅通道
   _redis.unsubscribe(user.getId());

   // 更新用户的状态信息
   if (user.getId() != -1)
   {
      user.setState("offline");
      _userModel.updateState(user);
      _redis.del_user(user.getId());
   }
}

// 一对一聊天
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp)
{
   int toid = js["toid"].get<int>();

   {
      lock_guard<mutex> lock(_connMutex);
      auto it = _userConnMap.find(toid);
      if (it != _userConnMap.end())
      {
         // toid在线，转发信息 服务器主动推送消息给toid用户
         it->second->send(js.dump());
         // string response_str = js.dump();
         // it->second->sendData(response_str.c_str(), response_str.size());
         return;
      }
   }

   // 查询toid是否在线  online --说明接收方可能在其他服务器节点在线，或内存连接池未及时更新，通过 Redis 发布消息（适用于分布式场景）
   /*

   */
  string value = _redis.get_state(toid);
    if(value != "null"){
        if(value == "online"){
            std::cout<<"user "<<toid<<" is online in redis"<<std::endl;
            _redis.publish(toid, js.dump());
        }
        else{
            std::cout<<"user "<<toid<<" is offline in redis"<<std::endl;
            _offlineMsgModel.insert(toid, js.dump());
        }
        return;
    }

   User user = _userModel.query(toid);
   if (user.getState() == "online")
   {  
      _redis.set_state(toid,"online");
      _redis.publish(toid, js.dump());
      return;
   }

   // toid不在线，存储离线消息

   _offlineMsgModel.insert(toid, js.dump());
   _redis.set_state(toid,"offline");
}

// 添加好友业务 msgid id  friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp)
{
   int userid = js["id"].get<int>();
   int friendid = js["friendid"].get<int>();

   // 存储好友信息
   _friendModel.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp)
{
   int userid = js["id"].get<int>();
   string name = js["groupname"];
   string desc = js["groupdesc"];

   // 存储新创建的群组信息
   Group group(-1, name, desc);
   if (_groupModel.createGroup(group))
   {
      // 存储群组创建人信息
      _groupModel.addGroup(userid, group.getId(), "creator");
      printf("your groupid is %d",group.getId());
   }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp)
{
   int userid = js["id"].get<int>();
   int groupid = js["groupid"].get<int>();
   _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp)
{
   int userid = js["id"].get<int>();
   int groupid = js["groupid"].get<int>();
   vector<int> useridvec = _groupModel.queryGroupsUsers(userid, groupid);
   lock_guard<mutex> lock(_connMutex);
   for (int id : useridvec)
   {

      auto it = _userConnMap.find(id);
      if (it != _userConnMap.end())
      {
         // 转发群消息
         it->second->send(js.dump());
         // string response_str = js.dump();
         // it->second->sendData(response_str.c_str(), response_str.size());
      }
      else
      {
         // 查询toid是否在线
         string value = _redis.get_state(id);
            if(value != "null"){
                if(value == "online"){
                    _redis.publish(id, js.dump());
                }
                else{
                _offlineMsgModel.insert(id, js.dump());
                }
                return;
         }
         User user = _userModel.query(id);
         if (user.getState() == "online")
         {
            _redis.set_state(id,"online");
            _redis.publish(id, js.dump());
         }
         else
         {
            // 存储离线群消息
            _redis.set_state(id,"offline");
            _offlineMsgModel.insert(id, js.dump());
         }
      }
   }
}

// 从redis信息队列中获取订阅的信息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
   //构造时自动锁定互斥锁，在析构时自动解锁互斥锁
   lock_guard<mutex> lock(_connMutex);
   //_userConnMap为map容器，用于存储在线用户的 ID 和对应的 TCP 连接指针
   auto it = _userConnMap.find(userid);
   if (it != _userConnMap.end())
   {
      //将数据发送给对端，即客户端
      it->second->send(msg);
      //it->second->sendData(msg.c_str(), msg.size());
      return;
   }

   // 存储该用户的离线信息
   _offlineMsgModel.insert(userid, msg);
}