#include "chatservice.h"

#include <muduo/base/Logging.h>

#include "public.h"
using namespace muduo;
#include <map>
#include <string>
#include <vector>
// 获取单例对象的接口函数
ChatService *ChatService::Instance() {
  // static对象是单例且线程安全的
  static ChatService service;
  return &service;
}

// 注册消息以及对应的Handler回调操作，msgid为1则是Login，msgid为2则是Register
ChatService::ChatService() {
  msgHandlerMap_.insert(
      {LOGIN_MSG, std::bind(&ChatService::Login, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {REGISTER_MSG, std::bind(&ChatService::Register, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
  msgHandlerMap_.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup,
                                                     this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {LOGINOUT_MSG, std::bind(&ChatService::loginOut, this, _1, _2, _3)});
  // 连接redis
  if (redis_.connect()) {
    // 设置上报消息的回调
    redis_.init_notify_handler(
        std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
  }
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid) {
  auto it = msgHandlerMap_.find(msgid);
  if (it == msgHandlerMap_.end()) {
    return [=](const TcpConnectionPtr &conn, json &js, Timestamp time) {
      LOG_ERROR << "msgid:" << msgid << "can not find handler!";
    };
  } else {
    return msgHandlerMap_[msgid];
  }
}

// 处理登录业务
void ChatService::Login(const TcpConnectionPtr &conn, json &js,
                        Timestamp time) {
  int id = js["id"].get<int>();
  string pwd = js["password"];
  User user = userModel_.query(id);
  if (user.getId() == id && user.getPwd() == pwd) {
    if (user.getState() == "online") {  // 已经在线，登录失败
      json response;
      response["msgid"] = LOGIN_MSG_ACK;
      response["errno"] = 2;
      response["errmsg"] = "该账号已经登录！不允许重复登录";
      conn->send(response.dump());
    } else {
      // 登录成功
      {  // 线程安全地访问用户连接表
        lock_guard<mutex> lock(connMutex_);
        connMap_.insert({id, conn});
      }
      // 向redis订阅channel
      redis_.subscribe(id);

      user.setState("online");
      userModel_.updateState(user);

      json response;
      response["msgid"] = LOGIN_MSG_ACK;
      response["errno"] = 0;
      response["id"] = user.getId();
      response["name"] = user.getName();
      // 查询该用户是否有离线消息
      vector<string> msg = offlineMsgModel_.query(user.getId());
      if (!msg.empty()) {
        response["offlinemsg"] = msg;
        offlineMsgModel_.remove(user.getId());
      }
      // 查询该用户的好友信息并返回
      vector<User> friends = friendModel_.query(id);
      if (!friends.empty()) {
        vector<string> users;
        for (User &user : friends) {
          json js;
          js["id"] = user.getId();
          js["name"] = user.getName();
          js["state"] = user.getState();
          users.push_back(js.dump());
        }
        response["friends"] = users;
      }

      // 查询组的信息并返回
      vector<Group> groups = groupModel_.queryGroups(id);
      if (!groups.empty()) {
        vector<string> vec;

        for (Group &group : groups) {
          json js;
          js["groupid"] = group.getId();
          js["groupname"] = group.getName();
          js["groupdesc"] = group.getDesc();
          vector<GroupUser> users = group.getUsers();
          vector<string> userinfo;
          for (GroupUser &user : users) {
            json js1;
            js1["id"] = user.getId();
            js1["name"] = user.getName();
            js1["state"] = user.getState();
            js1["role"] = user.getRole();
            userinfo.push_back(js1.dump());
          }
          js["users"] = userinfo;
          vec.push_back(js.dump());
        }
        response["groups"] = vec;
      }
      conn->send(response.dump());
    }
  } else {
    // 登录失败
    json response;
    response["msgid"] = LOGIN_MSG_ACK;
    response["errno"] = 1;
    response["errmsg"] = "账号或者密码错误！";
    conn->send(response.dump());
  }
}

// 处理注册业务
void ChatService::Register(const TcpConnectionPtr &conn, json &js,
                           Timestamp time) {
  string name = js["name"];
  string pwd = js["password"];

  User user;
  user.setName(name);
  user.setPwd(pwd);
  bool state = userModel_.insert(user);
  if (state) {
    // 注册成功
    json response;
    response["msgid"] = REGISTER_MSG_ACK;
    response["errno"] = 0;
    response["id"] = user.getId();
    conn->send(response.dump());
  } else {
    // 注册失败
    json response;
    response["msgid"] = REGISTER_MSG_ACK;
    response["errno"] = 1;
    conn->send(response.dump());
  }
}

// 客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn) {
  User user;
  {
    lock_guard<mutex> lock(connMutex_);
    for (auto it = connMap_.begin(); it != connMap_.end(); it++) {
      if (it->second == conn) {
        user.setId(it->first);
        connMap_.erase(it);
        break;
      }
    }
  }
  if (user.getId() != -1) {
    user.setState("offline");
    userModel_.updateState(user);
  }
}

// 一对一聊天业务
/**
 * id:int(发送消息的用户的id)
 * from:string(发送消息的用户的名字)
 * time:string(发送消息时的时间)
 * to：int(接受消息的用户的id)
 * msg:string(消息)
 */
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js,
                          Timestamp time) {
  int toid = js["to"].get<int>();
  // 同一服务器上
  {
    lock_guard<mutex> lock(connMutex_);
    auto it = connMap_.find(toid);
    if (it != connMap_.end()) {
      // 用户在线，转发消息
      it->second->send(js.dump());
      return;
    }
  }
  // 不同服务器上
  User user = userModel_.query(toid);
  if (user.getState() == "online") {
    redis_.publish(toid, js.dump());
    return;
  }
  // 用户不在线，存储离线消息
  offlineMsgModel_.insert(toid, js.dump());
}
// 重置数据库
void ChatService::reset() { userModel_.resetState(); }

// 添加好友 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js,
                            Timestamp time) {
  int userid = js["id"].get<int>();
  int friendid = js["friendid"].get<int>();

  // 存储好友信息
  friendModel_.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js,
                              Timestamp time) {
  int userid = js["id"].get<int>();
  string name = js["groupname"];
  string desc = js["groupdesc"];
  Group group(-1, name, desc);

  if (groupModel_.createGroup(group)) {
    groupModel_.addGroup(userid, group.getId(), "creator");
  }
}
// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js,
                           Timestamp time) {
  int userid = js["id"].get<int>();
  int groupid = js["groupid"].get<int>();
  groupModel_.addGroup(userid, groupid, "normal");
}
// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js,
                            Timestamp time) {
  int userid = js["id"].get<int>();
  int groupid = js["groupid"].get<int>();
  vector<int> useridVec = groupModel_.queryGroupUsers(userid, groupid);
  lock_guard<mutex> lock(connMutex_);
  for (int id : useridVec) {
    auto it = connMap_.find(id);
    if (it != connMap_.end()) {
      // 转发群消息
      it->second->send(js.dump());
    } else {
      // 不同服务器上
      User user = userModel_.query(id);
      if (user.getState() == "online") {
        redis_.publish(id, js.dump());
        return;
      } else {  // 不在线
        offlineMsgModel_.insert(id, js.dump());
      }
    }
  }
}

// 注销业务
void ChatService::loginOut(const TcpConnectionPtr &conn, json &js,
                           Timestamp time) {
  int userid = js["id"].get<int>();
  {
    lock_guard<mutex> lock(connMutex_);
    auto it = connMap_.find(userid);
    if (it != connMap_.end()) connMap_.erase(it);
  }
  User user(userid);
  userModel_.updateState(user);
  redis_.unsubscribe(userid);
}

// redis回调处理
void ChatService::handleRedisSubscribeMessage(int userid, string msg) {
  lock_guard<mutex> lock(connMutex_);
  auto it = connMap_.find(userid);
  if (it != connMap_.end()) {
    it->second->send(msg);
    return;
  }
  offlineMsgModel_.insert(userid, msg);
}