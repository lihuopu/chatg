#include "usermodel.hpp"
#include "db.h"

// User 表的增加方法
bool UserModel::insert(User &user)
{
    // // 1.组装sql语句
    // char sql[1024] = {0};
    // sprintf(sql, "insert into user(name,password,state) values('%s','%s','%s')",
    //         user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

    // MySQL mysql;
    // // MYSQL* m_sql;
    // // fix: 修复update之前提前释放sql回连接池的问题
    // MYSQL* m_sql = nullptr;
    // SqlConnRAII sql_raii(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //     if (mysql.update(sql))
    //     {
    //         // 获取插入成功的用户数据生成的主键id
    //         user.setId(mysql_insert_id(mysql.getConnection()));
    //         return true;
    //     }
    // }
    // return false;

    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into user(name,password,state) values('%s','%s','%s')",
            user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

    // 直接从连接池获取连接，不使用 MySQL 类
    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn,  SqlConnPool::Instance());
    
    if (conn == nullptr) {
        printf("获取 MySQL 连接失败");
        return false;
    }

    // 执行 SQL
    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "执行 SQL 失败: %s\n", mysql_error(conn));
        return false;
    }

    // 获取插入成功的用户数据生成的主键id
    user.setId(mysql_insert_id(conn));
    return true;
}

// 根据用户号码查询用户信息
User UserModel::query(int id)
{
    // // 1.组装sql语句
    // char sql[1024] = {0};
    // sprintf(sql, "select * from user where id = %d", id);

    // MySQL mysql;
    // MYSQL* m_sql;
    // SqlConnRAII sql_raii(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //     MYSQL_RES *res = mysql.query(sql);//查询结果指针
    //     if (res != nullptr)
    //     {
    //         MYSQL_ROW row = mysql_fetch_row(res);//使用 mysql_fetch_row 函数从结果集中获取第一行数据，并将其存储在 MYSQL_ROW 类型的变量 row 中
    //         if (row != nullptr)
    //         {
    //             User user;
    //             user.setId(atoi(row[0]));
    //             user.setName(row[1]);
    //             user.setPwd(row[2]);
    //             user.setState(row[3]);
    //             mysql_free_result(res);//释放结果集内存
    //             return user;
    //         }
    //     }
    // }
    // return User();

    User user;  // 默认构造的空用户
    char sql[1024] = {0};
    sprintf(sql, "select * from user where id = %d", id);

    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return user;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "执行 SQL 失败: %s\n", mysql_error(conn));
        return user;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (res != nullptr && mysql_num_rows(res) > 0) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row != nullptr) {
            user.setId(atoi(row[0]));
            user.setName(row[1] ? row[1] : "");
            user.setPwd(row[2] ? row[2] : "");
            user.setState(row[3] ? row[3] : "");
        }
        mysql_free_result(res);
    }

    return user;

}

// 更新用户的状态信息
bool UserModel::updateState(User user)
{
    // // 1.组装sql语句
    // char sql[1024] = {0};
    // sprintf(sql, "update user set state = '%s' where id = '%d'", user.getState().c_str(), user.getId());

    // MySQL mysql;
    // MYSQL* m_sql;
    // SqlConnRAII sql_raii(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //     if (mysql.update(sql))
    //     {
    //         // 获取插入成功的用户数据生成的主键id
    //         return true;
    //     }
    // }
    // return false;
    char sql[1024] = {0};
    sprintf(sql, "UPDATE user SET state = '%s' WHERE id = %d", 
            user.getState().c_str(), user.getId());

    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return false;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "执行 SQL 失败: %s\n", mysql_error(conn));
        return false;
    }

    return mysql_affected_rows(conn) > 0;  // 确保至少有一行被更新
}

// 重置用户的状态信息
void UserModel::resetState()
{
    const char* sql = "UPDATE user SET state = 'offline' WHERE state = 'online'";

    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "执行 SQL 失败: %s\n", mysql_error(conn));
    }
}