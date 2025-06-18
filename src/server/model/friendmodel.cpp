#include "friendmodel.hpp"
#include "db.h"
#include <cstdio>
#include<vector>

using namespace std;

// 添加好友关系
void FriendModel::insert(int userid, int friendid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d,'%d')", userid, friendid);
    MYSQL* conn = nullptr;
    SqlConnRAII temp(&conn, SqlConnPool::Instance());
    // MySQL mysql;
    // if (mysql.connect(m_sql))
    // {
    //     mysql.update(sql);
    // }

    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "执行 SQL 失败: %s\n", mysql_error(conn));
        return;
    }
}

// 返回用户好友列表  friendid
vector<User> FriendModel::query(int userid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.name,a.state from user a inner join friend b on b.friendid = a.id where b.userid=%d", userid);

    vector<User> vec;
    //MySQL mysql;
    MYSQL* conn = nullptr;
    SqlConnRAII temp(&conn, SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //     MYSQL_RES *res = mysql.query(sql);
    //     if (res != nullptr)
    //     {
    //         MYSQL_ROW row;
    //         // 把userid用户的所有的离线信息放入vex中返回
    //         while ((row = mysql_fetch_row(res)) != nullptr)
    //         {
    //             User user;
    //             user.setId(atoi(row[0]));
    //             user.setName(row[1]);
    //             user.setState(row[2]);
    //             vec.push_back(user);
    //         }
    //         mysql_free_result(res);
    //         return vec;
    //     }
    // }
    // return vec;
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return vec;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "执行 SQL 失败: %s\n", mysql_error(conn));
        return vec;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            User user;
            user.setId(row[0] ? atoi(row[0]) : 0);          // 处理空值
            user.setName(row[1] ? row[1] : "");            // 防止 NULL 指针
            user.setState(row[2] ? row[2] : "offline");     // 设置默认状态
            vec.push_back(user);
        }
        mysql_free_result(res);
    }

    return vec;
}