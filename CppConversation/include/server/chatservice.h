#ifndef CHATSERVICE_H
#define CHATSERVICE_H
#include <functional>
#include <mutex>
#include <unordered_map>

#include "friendmodel.h"
#include "groupmodel.h"
#include "json.hpp"
#include "muduo/net/TcpConnection.h"
#include "offlinemessagemodel.h"
#include "redis.hpp"
#include "usermodel.h"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;
// 处理消息事件的业务方法类型
using MsgHandler =
    std::function<void(const TcpConnectionPtr &, json &js, Timestamp)>;

class ChatService {
 public:
  // 获取单例对象的接口函数
  static ChatService *Instance();
  // 处理登录业务
  void Login(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 处理注册业务
  void Register(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 一对一聊天业务
  void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 获取消息对应的处理器
  MsgHandler getHandler(int msgid);
  // 处理客户端异常退出
  void clientCloseException(const TcpConnectionPtr &conn);
  // 重置数据库
  void reset();
  // 添加好友
  void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 创建群组业务
  void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 加入群组业务
  void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 群组聊天业务
  void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 注销业务
  void loginOut(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // redis回调处理
  void handleRedisSubscribeMessage(int, string);

 private:
  ChatService();
  // 存储消息id和其对应的业务处理方法
  unordered_map<int, MsgHandler> msgHandlerMap_;
  // 存储用户id和其对应的连接（已连接才存储）
  unordered_map<int, TcpConnectionPtr> connMap_;
  // 保证connMap_的线程安全
  mutex connMutex_;
  // 数据操作类对象
  UserModel userModel_;
  OfflineMsgModel offlineMsgModel_;
  friendModel friendModel_;
  GroupModel groupModel_;
  // redis操作对象
  Redis redis_;
};

#endif