#include "sqlconnpool.h"
using namespace std;


SqlConnPool::SqlConnPool() {
    useCount_ = 0;  //初始化已使用连接数
    freeCount_ = 0; //初始化空闲连接数
}

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;//保证单例模式指针
    return &connPool;
}


/*
    * 初始化数据库连接池
    * @param host 数据库主机地址
    * @param port 数据库端口号
    * @param user 数据库用户名
    * @param pwd 数据库密码
    * @param dbName 要连接的数据库名
    * @param connSize 连接池初始大小（默认10）
*/
void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    //
    std::lock_guard<std::mutex> locker(mtx_);  // 添加锁保护
    int successCount = 0;
    
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = mysql_init(nullptr);
        if (!sql) {
            fprintf(stderr, "MySql init error at index %d!\n", i);
            continue;
        }
        //建立实际的数据库连接
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {
            fprintf(stderr, "MySql Connect error at index %d: %s\n", i, mysql_error(sql));  // 输出具体错误
            mysql_close(sql);  // 关闭未成功的连接
            continue;
        }
        connQue_.push(sql);
        successCount++;
    }
    MAX_CONN_ = successCount;  //最大的连接数
    printf("Successfully created %d MySQL connections\n", MAX_CONN_);

    /*
        int sem_init(sem_t *sem, int pshared, unsigned int value);
        功能：对信号量对象进行初始化。
        参数：
        sem：指向信号量的指针。
        pshared：若为 0，则信号量在进程线程间共享；若为非零值，则在进程间共享。
        value：信号量的初始值，用于表示可用资源的数量。
        int sem_wait(sem_t *sem);
        若信号量的值大于 0，将其减 1 后继续执行。
        若信号量的值为 0，线程会被阻塞，直到信号量的值大于 0
        int sem_post(sem_t *sem);
        使信号量的值加 1。
        若有线程因等待该信号量而被阻塞，会唤醒其中一个线程。
    */
   if (MAX_CONN_ > 0) {
    sem_init(&semId_, 0, MAX_CONN_);  // 初始化信号量为实际连接数
    } else {
        fprintf(stderr, "Failed to create any MySQL connections!\n");
    } //初始化信号量，初始值为最大连接数
}

//从连接池获取一个可用的数据库连接
MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        printf("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);//// 等待信号量（减少可用连接计数
    {
        lock_guard<mutex> locker(mtx_);//加锁保护队列
        sql = connQue_.front();
        connQue_.pop();//移除队首元素
    }
    return sql;
}

//将数据库连接释放回连接池
void SqlConnPool::FreeConn(MYSQL* sql) {
    
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_);//释放信号量（增加可用连接计数）
}

//关闭连接池并释放所有数据库连接资源
void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end();        
}

//获取当前空闲连接的数量
int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}

//析构函数 - 调用ClosePool释放资源
SqlConnPool::~SqlConnPool() {
    ClosePool();
}