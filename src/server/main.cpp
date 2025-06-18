#include "chatserver.hpp"
#include "chatservice.hpp"
#include <signal.h>
#include <iostream>
#include "sqlconnpool.h"
using namespace std;

// 处理服务器ctrl+c 结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }

     string sql_host = "127.0.0.1";
     int sql_port = 3306;
     string sql_user = "root";
     string sql_password = "kang0904";
     string sql_dbname = "chat";
     int sql_connSize = 10;

    SqlConnPool::Instance()->Init(sql_host.c_str(),sql_port,sql_user.c_str(),sql_password.c_str(),sql_dbname.c_str(),sql_connSize);
    // 解析通过命令行参数传递的ip和port
    //eg:./ChatServer 127.0.0.1 6000
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    //SIGINT 为信号编号，表示用户按下 Ctrl + C 时产生的中断信号。当程序接收到 SIGINT 信号时，会调用 resetHandler 函数进行处理
    signal(SIGINT, resetHandler);

    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();

    return 0;
}
