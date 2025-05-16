# 基于muduo网络库的集群聊天项目
本项目运行在linux（Ubuntu 24.04，wsl2）环境下
# 依赖
## 基础版
**muduo**直接在Ubuntu下安装即可，安装这个库需要有一些前置库，问ai即可<br>

**json**作为头文件放在了项目中，来自https://github.com/nlohmann/json.git<br>

**mysql:** 数据库的名字是chat，建表的文件是chat.sql,数据库的配置在/include/db/db.hpp，把开头的四个string改成自己的数据库配置即可<br>

运行bin中的两个文件，记得带ip和port的参数
## 高级版
**nginx**: 为客户端提供负载均衡，下载源码https://github.com/nginx/nginx，在release中找到1.28.0.tar.gz<br>
将其解压后进入文件夹依次执行下面的命令
- ./configure --with-stream
- make && make install
- cd /usr/local/nginx/conf
此时在nginx中的配置文件夹里，此时配置nginx.conf配置文件，在其中添加stream配置，改配置使得nginx挂载127.0.0.1:6000 127.0.0.1:6002两个服务端，而发给8000端口的消息会轮番发给这两个服务端，之后所有客户端都连接8000端口
```vim
events {
    这里是nginx自己的配置
}
stream{
    upstream MyServer{
            server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
            server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
    }
    server {
        proxy_connect_timeout 1s;
        #proxy_timeout 3s;
        listen 8000;
        proxy_pass MyServer;
        tcp_nodelay on;
    }
}
http {
    这里是nginx自己的配置
}
```
配置好后保存退出运行nginx服务器，此时运行两个服务端，再用多个客户端连接到8000端口，会发现多个客户端的轮流连接两个服务器，达到负载均衡<br>

**redis**：消息队列，使得用户可以跨客户端通信，安装redis和hiredis，redis可以直接通过命令安装，hiredis的安装过程与nginx的安装过程相似，可以问ai
## 运行本项目
`cd build && cmake ..`<br>
`make`
