#ifndef DB_H
#define DB_H
#include <muduo/base/Logging.h>
#include <mysql/mysql.h>

#include <string>
using namespace std;
// 数据库配置信息
static string server = "127.0.0.1";
static string user = "root";
static string password = "root";
static string dbname = "chat";
// 数据库操作类
class MySQL {
 public:
  // 初始化数据库连接
  MySQL() { _conn = mysql_init(nullptr); }
  // 释放数据库连接资源
  ~MySQL() {
    if (_conn != nullptr) mysql_close(_conn);
  }
  // 连接数据库
  bool connect() {
    // LOG_INFO << "mysql connecting!";
    MYSQL *p =
        mysql_real_connect(_conn, server.c_str(), user.c_str(),
                           password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if (p == nullptr) LOG_INFO << "mysql connect failed!";
    if (p != nullptr) {
      LOG_INFO << "mysql connect succeed!";
      mysql_query(_conn, "set names gbk");
    }
    return p;
  }
  // 更新操作
  bool update(string sql) {
    if (mysql_query(_conn, sql.c_str())) {
      LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败!";
      return false;
    }
    return true;
  }
  // 查询操作
  MYSQL_RES *query(string sql) {
    if (mysql_query(_conn, sql.c_str())) {
      LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败!";
      return nullptr;
    }
    return mysql_use_result(_conn);
  }
  // 获取连接
  MYSQL *getConnection() { return _conn; }

 private:
  MYSQL *_conn;
};
#endif