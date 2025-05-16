#ifndef USERMODEL_H
#define USERMODEL_H
#include "user.h"
// User表的数据操作类
class UserModel {
 public:
  bool insert(User& user);
  User query(int id);
  void updateState(User& user);
  void resetState();
};

#endif