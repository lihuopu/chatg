#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/**
 * RAII (Resource Acquisition Is Initialization) 风格的数据库连接封装
 * 自动管理数据库连接的生命周期，确保资源正确释放
 */
class SqlConnRAII {
public:
    /**
     * 构造函数 - 从连接池获取数据库连接
     *  sql 双重指针，用于存储获取的数据库连接
     *  connpool 数据库连接池指针
     */
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        *sql = connpool->GetConn();  // 从连接池获取连接
        sql_ = *sql;                 // 保存连接指针
        connpool_ = connpool;        // 保存连接池指针
    }
    
    /**
     * 析构函数 - 自动将连接释放回连接池
     */
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); }  // 释放连接到连接池
    }
    
private:
    MYSQL *sql_;           // 数据库连接指针
    SqlConnPool* connpool_; // 数据库连接池指针
};

#endif //SQLCONNRAII_H