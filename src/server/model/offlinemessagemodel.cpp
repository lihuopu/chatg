#include "offlinemessagemodel.hpp"
#include "db.h"
#include <vector>
#include <cstdio>

using namespace std;

// 存储用户的离线信息
void OfflineMsgModel::insert(int userid,  string msg)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage values(%d,'%s')", userid, msg.c_str());

    // MySQL mysql;
    // MYSQL* m_sql;
    // SqlConnRAII temp(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //     mysql.update(sql);
    // }

    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "插入离线消息失败: %s\n", mysql_error(conn));
    }
}

// 删除用户的离线信息
void OfflineMsgModel::remove(int userid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid =%d", userid);

    // MySQL mysql;
    // MYSQL* m_sql;
    // SqlConnRAII temp(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {

    //     mysql.update(sql);
    // }
    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "删除离线消息失败: %s\n", mysql_error(conn));
    }
}

// 查询用户的离线消息
vector<string> OfflineMsgModel::query(int userid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d", userid);
    vector<string> vec;

    // MySQL mysql;
    // MYSQL* m_sql;
    // SqlConnRAII temp(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //     MYSQL_RES *res = mysql.query(sql);
    //     if (res != nullptr)
    //     {
    //         MYSQL_ROW row;
    //         // 把userid用户的所有的离线信息放入vex中返回
    //         while ((row = mysql_fetch_row(res)) != nullptr)
    //         {
    //             vec.push_back(row[0]);
    //         }
    //         mysql_free_result(res);
    //         return vec;
    //     }
    // }
    // return vec;
    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return vec;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "查询离线消息失败: %s\n", mysql_error(conn));
        return vec;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            if (row[0]) {
                vec.push_back(row[0]);
            }
        }
        mysql_free_result(res);
    }

    return vec;
}