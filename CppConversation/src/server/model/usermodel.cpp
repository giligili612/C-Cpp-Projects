#include "usermodel.h"

#include "db.hpp"
// 增加方法
bool UserModel::insert(User& user) {
  // 组装sql语句
  char sql[1024] = {0};
  sprintf(
      sql, "insert into user(name, password, state) values('%s','%s','%s');",
      user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());
  MySQL mysql;
  LOG_INFO << "insert: mysql connect begin!\n";
  if (mysql.connect()) {
    LOG_INFO << "insert: mysql connect succeed!\n";
    if (mysql.update(sql)) {
      // 获取插入成功的用户数据生成的主键id
      user.setId(mysql_insert_id(mysql.getConnection()));
      return true;
    }
  }
  return false;
}

// 查询方法
User UserModel::query(int id) {
  // 组装sql语句
  char sql[1024] = {0};
  sprintf(sql, "select * from user where id = %d", id);
  MySQL mysql;
  if (mysql.connect()) {
    MYSQL_RES* res = mysql.query(sql);
    if (res != nullptr) {
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row != nullptr) {
        User user;
        user.setId(atoi(row[0]));
        user.setName(row[1]);
        user.setPwd(row[2]);
        user.setState(row[3]);
        mysql_free_result(res);
        return user;
      }
    }
  }
  return User();
}

// 修改方法（更新）
void UserModel::updateState(User& user) {
  // 组装sql语句
  char sql[1024] = {0};
  sprintf(sql, "update user set state = '%s' where id = '%d'",
          user.getState().c_str(), user.getId());
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  } else {
    LOG_ERROR << "sql update error!";
  }
}

void UserModel::resetState() {
  // 组装sql语句
  char sql[1024] = {0};
  sprintf(sql, "update user set state = 'offline'");
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  } else {
    LOG_ERROR << "sql update error!";
  }
}
