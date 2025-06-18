#include "groupmodel.hpp"
#include "db.h"
#include <vector>
#include <cstdio>
#include <string>
#include <algorithm>

// 创建群组
bool GroupModel::createGroup(Group &group)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname,groupdesc) values('%s','%s')",
            group.getName().c_str(), group.getDesc().c_str());

    // MySQL mysql;
    // MYSQL* m_sql;
    // SqlConnRAII temp(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //     if (mysql.update(sql))
    //     {
    //         group.setId(mysql_insert_id(mysql.getConnection()));
    //         return true;
    //     }
    // }
    // return false;
    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return false;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "创建群组失败: %s\n", mysql_error(conn));
        return false;
    }

    group.setId(mysql_insert_id(conn));
    return true;
}

// 加入群组
void GroupModel::addGroup(int userid, int groupid, string role)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d,%d,'%s')",
            groupid, userid, role.c_str());

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
        fprintf(stderr, "加入群组失败: %s\n", mysql_error(conn));
    }
}

//查询用户所在群组信息
vector<Group>GroupModel::queryGroups(int userid){

    // 1.先根据userid在groupuser表中查询出该用户所属的群组信息
    // 2.在根据群组信息,查询属于该群组的所有用户的userid,并且和user表进行多表联合查询,查出用户的详细信息
 
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from allgroup a inner join groupuser b on a.id = b.groupid where b.userid=%d", userid);

    vector<Group> groupvec;
    // MySQL mysql;
    // MYSQL* m_sql;
    // SqlConnRAII temp(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //     MYSQL_RES *res = mysql.query(sql);
    //     if (res != nullptr)
    //     {
    //         MYSQL_ROW row;
    //         // 把userid所有群组信息
    //         while ((row = mysql_fetch_row(res)) != nullptr)
    //         {
    //             Group group;
    //             group.setId(atoi(row[0]));
    //             group.setName(row[1]);
    //             group.setDesc(row[2]);
    //             groupvec.push_back(group);
    //         }
    //         mysql_free_result(res);
    //     }
    // }
    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return groupvec;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "查询群组列表失败: %s\n", mysql_error(conn));
        return groupvec;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            Group group;
            group.setId(row[0] ? atoi(row[0]) : 0);
            group.setName(row[1] ? row[1] : "");
            group.setDesc(row[2] ? row[2] : "");
            groupvec.push_back(group);
        }
        mysql_free_result(res);
    }
    //查询群组的用户信息
    for(Group & group:groupvec){
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from user a inner join groupuser b on b.userid = a.id where b.groupid=%d", group.getId());
        // MYSQL_RES *res = mysql.query(sql);
        // if (res != nullptr)
        // {
        //     MYSQL_ROW row;
        //     // 把userid所有群组信息
        //     while ((row = mysql_fetch_row(res)) != nullptr)
        //     {
        //         GroupUser user;
        //         user.setId(atoi(row[0]));
        //         user.setName(row[1]);
        //         user.setState(row[2]);
        //         user.setRole(row[3]);
        //         group.getUsers().push_back(user);
        //     }
        //     mysql_free_result(res);
        // }
        if (mysql_query(conn, sql) != 0) {
            fprintf(stderr, "查询群组用户失败: groupid=%d, %s\n", group.getId(), mysql_error(conn));
            continue;
        }

        res = mysql_store_result(conn);
        if (res != nullptr) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                GroupUser user;
                user.setId(row[0] ? atoi(row[0]) : 0);
                user.setName(row[1] ? row[1] : "");
                user.setState(row[2] ? row[2] : "offline");
                user.setRole(row[3] ? row[3] : "member");
                group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return groupvec;
}

//根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其它成员群发信息
vector<int> GroupModel::queryGroupsUsers(int userid ,int groupid)
{
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d",
            groupid,userid);
    vector<int> idvec;
    // MySQL mysql;
    // MYSQL * m_sql;
    // SqlConnRAII temp(&m_sql,  SqlConnPool::Instance());
    // if (mysql.connect(m_sql))
    // {
    //    MYSQL_RES * res = mysql.query(sql);
    //    if(res != nullptr){
    //     MYSQL_ROW row;
    //     while((row = mysql_fetch_row(res)) != nullptr){
    //         idvec.push_back(atoi(row[0]));
    //     }
    //     mysql_free_result(res);
    //    }
    // }
    MYSQL* conn = nullptr;
    SqlConnRAII sql_raii(&conn, SqlConnPool::Instance());
    
    if (conn == nullptr) {
        fprintf(stderr, "获取 MySQL 连接失败\n");
        return idvec;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "查询群组用户ID失败: %s\n", mysql_error(conn));
        return idvec;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            if (row[0]) {
                idvec.push_back(atoi(row[0]));
            }
        }
        mysql_free_result(res);
    }
    
    return idvec;
}