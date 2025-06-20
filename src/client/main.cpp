#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <functional>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>
#include "group.hpp"
#include "user.hpp"
#include "public.hpp"
#include "clientssl.h"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};
// 接收线程
void readTaskHandler(SSL *ssl);
// 获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime();
// 主聊天页面程序
void mainMenu(SSL*);
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

int clientfd;

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket(套接字)
    //函数原型：int socket(int domain, int type, int protocol);
    // domain - 套接字使用的协议族 type - 套接字类型 protocol - 具体使用协议
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    SSL *ssl = nullptr;
    //检查套接字创建是否成功
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip + port
    //sockaddr_in  <netinet/in.h> 头文件中的结构体，专门用于存储 IPv4 地址族的网络地址信息。
    sockaddr_in server;
    //将结构体内存区域设置为0
    memset(&server, 0, sizeof(sockaddr_in));

    //设置地址族，端口号，ip地址
    //htons()与inet_addr()分别将主机字节序转换为网络字节序
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    else
    {
        // 初始化SSL连接，使用指定的证书和密钥文件
        // - "cert.pem": 证书文件路径，用于验证身份
        // - "key.pem": 私钥文件路径，用于加密通信
        // - SSL_MODE_CLIENT: 设置SSL为客户端模式
        // - clientfd: 客户端套接字文件描述符
        ssl = sync_initialize_ssl("/home/ca-certificates/certs/ca.cert.pem","/root/ca-certificates/server-cert.pem", "/root/ca-certificates/server-private.key", SSL_MODE_CLIENT, clientfd);
        if (nullptr == ssl)
        {
            cerr << "ssl initialize error" << endl;
            close(clientfd);
            exit(-1);
        }
    }
    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);
    // 连接服务器成功，启动接收子线程
    std::thread readTask(readTaskHandler, ssl); // pthread_create
    //将线程分离，分离后的线程会在后台独立运行
    //主线程接收用户输入并发送数据，子线程使其在后台持续运行从服务器接受数据，根据数据类型实现对应处理
    readTask.detach();                               // pthread_detach

    // main线程用于接收用户输入，负责发送数据-主线程
    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "===============" << endl;
        cout << "1.login" << endl;
        cout << "2.register" << endl;
        cout << "3.quit" << endl;
        cout << "===============" << endl;
        cout << "choice";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1:
        { // login业务
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            //string request = js.dump();

            if (!SSL_is_init_finished(ssl)) {
                cerr << "错误：SSL握手未完成，无法发送数据" << endl;
                break;
            }

            string request = js.dump();
            int len = SSL_write(ssl, request.c_str(), request.size()); // 改用request.size()避免多余空字节
            if (len <= 0) {
                int error = SSL_get_error(ssl, len);
                cerr << "SSL_write 失败，错误码：" << error << endl;
                ERR_print_errors_fp(stderr); // 输出OpenSSL底层错误
                continue;
            }

            g_isLoginSuccess = false;

            //int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            //int len = SSL_write(ssl, request.c_str(), strlen(request.c_str()) + 1);
            // 关键：检查SSL握手是否完成
            
            // if (len == -1)
            // {
            //     cerr << "send login msg error:" << request << endl;
            // }

            sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应信息后，通知这里

            if (g_isLoginSuccess)
            {
                // 进入聊天主菜单页面
                isMainMenuRunning = true;
                mainMenu(ssl);
            }
        }
        break;
        case 2:
        { // register业务
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            //int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            int len = SSL_write(ssl, request.c_str(), strlen(request.c_str()) + 1);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
                // cerr << "send reg msg error:" << strerror(errno) << endl;
            }
            
            sem_wait(&rwsem); // 等待信号量,子线程处理完注册信息会通知
        }
        break;
        case 3: // quit 业务
            //关闭客户端套接字clientfd,终止与服务器连接
            close(clientfd);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            //销毁信号量rwsem，释放相关资源
            sem_destroy(&rwsem);
            //退出程序
            exit(0);

        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }
    return 0;
}

// 处理注册的响应逻辑
void doRegResponse(json &responsejs)
{
    if (0 != responsejs["errno"].get<int>())
    { // 注册失败
        cerr << " name is already exist, register error!" << endl;
    }
    else
    { // 注册失败
        cout << " name register success, userid is" << responsejs["id"]
             << ",do not forget it!" << endl;
    }
}
// 处理登录的响应逻辑
void doLoginResponse(json &responsejs)
{
    if (0 != responsejs["errno"].get<int>())
    { // 登录失败
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else
    { // 登录成功
        // 记录当前用户的id和name
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        // 记录当前用户的好友列表信息
        //contains()是 JSON 库中的方法，用于检查 JSON 对象中是否包含指定的键
        if (responsejs.contains("friends"))
        {
            // 初始化
            g_currentUserFriendList.clear();
            vector<string> vec = responsejs["friends"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        if (responsejs.contains("groups"))
        {
            // 初始化
            g_currentUserGroupList.clear();

            vector<string> vec1 = responsejs["groups"];
            for (string &groupstr : vec1)
            {
                json grpjs = json::parse(groupstr);
                Group group;
                group.setId(grpjs["id"].get<int>());
                group.setName(grpjs["groupname"]);
                group.setDesc(grpjs["groupdesc"]);

                vector<string> vec2 = grpjs["users"];
                for (string &userstr : vec2)
                {
                    GroupUser user;
                    json js = json::parse(userstr);
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().push_back(user);
                }

                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线信息 个人聊天信息或者群组信息
        if (responsejs.contains("offlinemsg"))
        {
            vector<string> vec = responsejs["offlinemsg"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                // time + [id] + name + "said:" + xxx
                if (ONE_CHAT_MSG == js["msgid"].get<int>())
                {
                    cout << js["time"].get<string>() << "[" << js["id"] << "]"
                         << js["name"].get<string>() << "said:" << js["msg"].get<string>()
                         << endl;
                }
                else
                {
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]"
                         << js["name"].get<string>() << "said:" << js["msg"].get<string>()
                         << endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

// 子线程-接收线程
//client客户端套接字描述符，用于与服务器进行通信
void readTaskHandler(SSL *ssl)
{
    for (;;)
    {
        char buffer[1024] = {0};
        //recv函数用于接收服务器传输的数据
        //参数分别为客户端套接字 clientfd、存储数据缓冲区 buffer、缓冲区的最大长度 1024 以及标志位 0（表示使用默认行为）
        //int len = recv(clientfd, buffer, 1024, 0);
        int len = SSL_read(ssl, buffer, 1024);
        //-1——接收数据错误， 0——对端已经关闭连接
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            exit(-1);
        }

        // 接收Chatserver转发额数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << "[" << js["id"] << "]"
                 << js["name"].get<string>() << "said:" << js["msg"].get<string>()
                 << endl;
            continue;
        }
        if (GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]"
                 << js["name"].get<string>() << "said:" << js["msg"].get<string>()
                 << endl;
            continue;
        }
        if (LOGIN_MSG_ACK == msgtype)
        {
            doLoginResponse(js); // 处理登录响应的业务逻辑
            sem_post(&rwsem);    // 通知主线程，登录结果处理完成
            continue;
        }
        if (REG_MSG_ACK == msgtype)
        {
            doRegResponse(js);
            sem_post(&rwsem); // 通知主线程，登录结果处理完成
            continue;
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "==========login user=============" << endl;
    cout << "current login user => id :" << g_currentUser.getId() << "name:" << g_currentUser.getName() << endl;
    cout << "------------friend list----------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "-----------group list------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "=================================" << endl;
}

// 获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime()
{
    //获取当前时间点并将其转换为time_t类型,本质为时间戳，通常表示从某个固定的时间点（如 1970 年 1 月 1 日 00:00:00 UTC）到当前时间的秒数。
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    //将time_t类型的时间戳转换为本地时间的tm结构体，。tm 结构体包含了年、月、日、时、分、秒等时间信息
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    //其中 %02d 表示输出的整数宽度为 2，不足 2 位时在前面补 0
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

//"help" command handler
//void help(int fd = 0, string str = "");
void help(SSL *ssl = nullptr, string str = "");
//"chat" command handler
//void chat(int, string);
void chat(SSL*, string);
//"addfriend" command handler
//void addfriend(int, string);
void addfriend(SSL*, string);
//"creategroup" command handler
//void creategroup(int, string);
void creategroup(SSL*, string);
//"addgroup" command handler
//void addgroup(int, string);
void addgroup(SSL*, string);
//"groupchat" command handler
//void groupchat(int, string);
void groupchat(SSL*, string);
//"loginout" command handler
//void loginout(int, string);
void loginout(SSL*, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"loginout", "注销,格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(SSL*, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 主聊天页面程序
void mainMenu(SSL *ssl)
{
    help();

    char buffer[1024] = {0};
    //在login成功时已置为true
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {   
            //用来实现help,loginout命令
            command = commandbuf;
        }
        else
        {   
            //截取开头到冒号前作为命令输入
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应命令事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(ssl, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法
    }
}

//"help" command handler
void help(SSL*, string)
{
    cout << "show commnand list >>>" << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << ":" << p.second << endl;
    }
    cout << endl;
}
//"addfriend" command handler
void addfriend(SSL *ssl, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = SSL_write(ssl, buffer.c_str(), strlen(buffer.c_str()) + 1);
    //int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error ->" << buffer << endl;
    }
}
//"chat" command handler
void chat(SSL *ssl, string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = SSL_write(ssl, buffer.c_str(), strlen(buffer.c_str()) + 1);
    //int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error ->" << buffer << endl;
    }
}
//"creategroup" command handler
void creategroup(SSL *ssl, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;

    string buffer = js.dump();

    int len = SSL_write(ssl, buffer.c_str(), strlen(buffer.c_str()) + 1);
    //int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error ->" << buffer << endl;
    }
}
//"addgroup" command handler
void addgroup(SSL *ssl, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = SSL_write(ssl, buffer.c_str(), strlen(buffer.c_str()) + 1);
    //int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error ->" << buffer << endl;
    }
}
//"groupchat" command handler groupid:message
void groupchat(SSL *ssl, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string messages = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = messages;
    js["time"] = getCurrentTime();

    string buffer = js.dump();

    int len = SSL_write(ssl, buffer.c_str(), strlen(buffer.c_str()) + 1);
    //int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error ->" << buffer << endl;
    }
}
//"loginout" command handler
void loginout(SSL *ssl, string str)
{

    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = SSL_write(ssl, buffer.c_str(), strlen(buffer.c_str()) + 1);
    //int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send loginout msg error ->" << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}