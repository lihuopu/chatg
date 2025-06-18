#include "db.h"
#include <muduo/base/Logging.h>

// 数据库配置信息

// static string server = "127.0.0.1";
// static string user = "root";
// static string password = "kang0904";
// static string dbname = "chat";
 // 初始化数据库连接
 MySQL::MySQL()
 {
    //conn = mysql_init(nullptr);
 }

 // 释放数据库连接资源
 MySQL::~MySQL()
 {
    //  if (_conn != nullptr)
    //  {
    //      mysql_close(_conn);
    //  }
 }
 // 连接数据库
 bool MySQL::connect(MYSQL*m_sql){
     _conn = m_sql;
    //  MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(),
    //              password.c_str(), dbname.c_str(), 3306, nullptr, 0);
                 //初始化的 MYSQL 对象指针、服务器地址、用户名、密码、数据库名、端口号、参数 unix_socket、client_flag
     if (_conn= nullptr)
     {
         //c和c++代码默认为ASCII，如不设置，MySQL上拉下来的中文显示为？
         mysql_query(_conn, "set names gbk");
         LOG_INFO<<"connect mysql success!";
     }
    else{
        LOG_INFO<<"connect mysql fail!";
        return false;
    }
     return true;
 }

 // 更新操作
 bool MySQL::update(string sql)
 {
     if (mysql_query(_conn, sql.c_str()))
     {
         LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                  << sql << "更新失败！";//输出文件名，行号，具体的SQL语句
         return false;
     }
     return true;
 }
 // 查询操作
 MYSQL_RES *MySQL::query(string sql)
 {
     if (mysql_query(_conn, sql.c_str()))
     {
         LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                  << sql << "查询失败！";
         return nullptr;
     }
     return mysql_use_result(_conn);//从服务器获取查询结果
 }
 //获取连接
 MYSQL * MySQL::getConnection(){
    return _conn;
 }