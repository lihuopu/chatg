#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>

/*
    数据库连接池 - 负责管理MySQL数据库连接的创建。分配和回收
    使用单例模式：
    私有构造函数
    禁止外部直接实例化，确保只能通过类的静态方法获取实例。
    静态实例变量
    存储类的唯一实例，确保全局可见。
    线程安全
    在多线程环境中，需通过锁机制避免多个线程同时创建实例。
    懒汉模式：
    实例在第一次使用时创建（如 getInstance() 被调用时），而非程序启动时
    饿汉模式
    在类加载时立即创建实例，无需锁，线程安全但可能造成资源浪费。


*/
class SqlConnPool {
    public:
        static SqlConnPool *Instance(); //获取连接池的单例实例
        MYSQL *GetConn();//从连接池获取一个可用的数据库连接
        void FreeConn(MYSQL * conn);//将数据库连接释放回连接池
        int GetFreeConnCount();//获取当前空闲连接的数量
        
        //初始化数据库连接池
        void Init(const char* host, int port,
                  const char* user,const char* pwd, 
                  const char* dbName, int connSize);

        void ClosePool();//关闭连接池并释放所有数据库连接资源
    
    private:
        SqlConnPool();
        ~SqlConnPool();
    
        int MAX_CONN_;//最大连接数
        int useCount_;///当前用户数
        int freeCount_;//空闲用户数
    
        std::queue<MYSQL *> connQue_; //存储数据库连接的队列
        std::mutex mtx_;//互斥锁
        sem_t semId_;//信号量
    };
    



#endif