#ifndef GROUPMODEL_H
#define GROUPMODEL_H
#include <string>
#include <vector>

#include "group.h"

// 维护群组信息的操作接口方法
class GroupModel {
 public:
  // 创建群组
  bool createGroup(Group& group);
  // 加入群组
  void addGroup(int userid, int groupid, string role);
  // 查询用户所在群组信息
  vector<Group> queryGroups(int userid);
  // 根据群组查询组用户(除了查询之人的id)，主要用于给群友发消息
  vector<int> queryGroupUsers(int userid, int groupid);
};
#endif