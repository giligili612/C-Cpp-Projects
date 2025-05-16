#include "chatserver.h"

#include <functional>
#include <string>

#include "chatservice.h"
#include "json.hpp"

using namespace std;
using namespace placeholders;
using json = nlohmann::json;
ChatServer::ChatServer(EventLoop *loop, const InetAddress &listenAddr,
                       const string &nameArg)
    : server_(loop, listenAddr, nameArg), loop_(loop) {
  // 注册链接回调
  server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
  // 注册消息回调
  server_.setMessageCallback(
      std::bind(&ChatServer::onMessage, this, _1, _2, _3));
  // 设置线程数量
  server_.setThreadNum(4);
};

void ChatServer::Start() { server_.start(); }

void ChatServer::onConnection(const TcpConnectionPtr &conn) {
  // 用户断开连接
  if (!conn->connected()) {
    ChatService::Instance()->clientCloseException(conn);
    conn->shutdown();
  }
}
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buffer,
                           Timestamp time) {
  string buf = buffer->retrieveAllAsString();
  // 数据的反序列化
  json js = json::parse(buf);
  // 完全解耦网络模块的代码和业务模块的代码
  // 通过js["msgid"]获取业务handler
  auto msgHandler = ChatService::Instance()->getHandler(js["msgid"].get<int>());
  msgHandler(conn, js, time);
}