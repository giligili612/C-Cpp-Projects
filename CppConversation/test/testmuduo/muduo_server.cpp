#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace muduo;
using namespace muduo::net;

class ChatServer
{
public:
    ChatServer( EventLoop *loop, //事件循环
                const InetAddress& listenAddr, //套接字
                const string& nameArg) //服务器名字
    :server_(loop, listenAddr, nameArg)
    ,loop_(loop)
    {
        //给服务器注册用户连接的创建和断开回调
        server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
        //给服务器注册用户读写事件回调
        server_.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1,_2,_3));
        //设置服务器的线程数量,1个I/O,3个worker线程
        server_.setThreadNum(4);
    }
    //开启事件循环
    void Start(){
        server_.start();
    }
private:
    // 专门处理用户的连接创建和断开 epoll listenfd accept
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected())
            cout << conn->peerAddress().toIpPort() << "->"
            << conn->localAddress().toIpPort() << endl
            << "status online\n";
        else
            cout << conn->peerAddress().toIpPort() << "->"
            << conn->localAddress().toIpPort() << endl
            << "status offline\n";
    }

    //专门处理用户的读写事件
    void onMessage( const TcpConnectionPtr& conn, //连接
                    Buffer *buf,    //缓冲区
                    Timestamp time) //时间戳
    {
        string bufer = buf->retrieveAllAsString();
        cout << "recv data:" << bufer << "time:" 
        << time.toString() << endl;
        conn->send(bufer);
    }
    TcpServer server_;  // #1
    EventLoop* loop_;   // #2 epoll
};
//链接时顺序：-lpthread -lmuduo_net -lmuduo_base
int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 8080);
    ChatServer server(&loop, addr, "EchoServer");
    server.Start();
    loop.loop();
    return 0;
}