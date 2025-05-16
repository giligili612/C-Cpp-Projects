#include <chrono>
#include <ctime>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "json.hpp"
using namespace std;
using json = nlohmann::json;

#include <arpa/inet.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>

#include "group.h"
#include "public.h"
#include "user.h"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int);
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char* argv[]) {
  if (argc < 3) {
    cerr << "command invalid! example:./ChatClient 127.0.0.1 6000" << endl;
    exit(-1);
  }
  // 解析通过命令行参数传递的ip和port
  char* ip = argv[1];
  uint16_t port = atoi(argv[2]);

  // 创建client端的socket
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd == -1) {
    cerr << "socket create error" << endl;
    exit(-1);
  }

  // 填写client需要连接的server信息ip+port
  sockaddr_in server;
  memset(&server, 0, sizeof(sockaddr_in));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip);
  server.sin_port = htons(port);

  // client和server连接
  if (connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in)) == -1) {
    cerr << "connect server error " << endl;
    close(clientfd);
    exit(-1);
  }

  // 初始化读写线程通信用的信号量
  sem_init(&rwsem, 0, 0);

  // 连接服务器成功，启动接受子线程
  std::thread readTask(readTaskHandler, clientfd);
  readTask.detach();

  // main线程接收用户输入，发送数据
  for (;;) {
    // 显示首页面菜单 登录、注册、退出
    cout << "===================================" << endl;
    cout << "1.login" << endl;
    cout << "2.register" << endl;
    cout << "3.quit" << endl;
    cout << "===================================" << endl;
    cout << "choice:";
    int choice = 0;
    cin >> choice;
    cin.get();  // 清理缓冲区的回车

    switch (choice) {
      case 1: {  // 登录业务
        int id = 0;
        char pwd[50] = {0};
        cout << "userid:";
        cin >> id;
        cin.get();
        cout << "userpassword:";
        cin.getline(pwd, 50);

        json js;
        js["msgid"] = LOGIN_MSG;
        js["id"] = id;
        js["password"] = pwd;
        string request = js.dump();

        g_isLoginSuccess = false;
        int len =
            send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
        if (len == -1) {
          cerr << "send login msg error:" << request << endl;
        }
        sem_wait(&rwsem);  // 等待信号量，由子线程处理完登录的相应消息后通知这里
        if (g_isLoginSuccess) {
          isMainMenuRunning = true;
          mainMenu(clientfd);
        }
      } break;
      case 2: {  // 注册业务
        char name[50] = {0};
        char pwd[50] = {0};
        cout << "username:";
        cin.getline(name, 50);
        cout << "userpassword:";
        cin.getline(pwd, 50);

        json js;
        js["msgid"] = REGISTER_MSG;
        js["name"] = name;
        js["password"] = pwd;
        string request = js.dump();

        int len =
            send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
        if (len == -1) {
          cerr << "send register msg error!" << endl;
        }
        sem_wait(&rwsem);
      } break;
      case 3:  // 退出业务
        close(clientfd);
        sem_destroy(&rwsem);
        exit(0);
      default:
        cerr << "invalid input " << endl;
        break;
    }
  }
}
// 处理登录的响应逻辑
void doLoginResponse(json& responsejs);
// 处理注册的相应逻辑
void doRegResponse(json& responsejs);

// 接收线程，处理服务器发来的消息
void readTaskHandler(int clientfd) {
  for (;;) {
    char buffer[1024] = {0};
    int len = recv(clientfd, buffer, 1024, 0);
    if (len == -1 || len == 0) {
      close(clientfd);
      exit(-1);
    }

    // 接收ChatServer转发的数据，反序列化生成json数据对象
    json js = json::parse(buffer);
    int msgtype = js["msgid"].get<int>();
    // 点对点聊天的信息
    if (ONE_CHAT_MSG == msgtype) {
      cout << js["time"].get<string>() << " [" << js["id"] << "]"
           << js["name"].get<string>() << " said: " << js["msg"].get<string>()
           << endl;
      continue;
    }
    // 群聊信息
    if (GROUP_CHAT_MSG == msgtype) {
      cout << "Groupid[" << js["groupid"] << "]:" << js["time"].get<string>()
           << " User[" << js["id"] << "] " << js["name"].get<string>()
           << " said: " << js["msg"].get<string>() << endl;
      continue;
    }
    // 登录响应
    if (LOGIN_MSG_ACK == msgtype) {
      doLoginResponse(js);
      sem_post(&rwsem);
      continue;
    }
    // 注册相应
    if (REGISTER_MSG_ACK == msgtype) {
      doRegResponse(js);
      sem_post(&rwsem);
      continue;
    }
  }
}

// 处理注册的相应逻辑
void doRegResponse(json& responsejs) {
  if (responsejs["errno"].get<int>() != 0) {
    cerr << "name is already exist, register error!" << endl;
  } else {
    cout << "name register success, userid is " << responsejs["id"]
         << ", do not forget it!" << endl;
  }
}
// 处理登录的响应逻辑
void doLoginResponse(json& responsejs) {
  if (responsejs["errno"].get<int>() != 0) {
    cerr << responsejs["errmsg"] << endl;
    g_isLoginSuccess = false;
  } else {  // 登录成功
    g_currentUser.setId(responsejs["id"].get<int>());
    g_currentUser.setName(responsejs["name"]);
    // 如果有好友，存储好友列表信息
    if (responsejs.contains("friends")) {
      // 初始化
      g_currentUserFriendList.clear();

      vector<string> vec = responsejs["friends"];
      for (string& str : vec) {
        json js = json::parse(str);
        User user;
        user.setId(js["id"].get<int>());
        user.setName(js["name"]);
        user.setState(js["state"]);
        g_currentUserFriendList.push_back(user);
      }
    }

    // 记录当前用户的群组列表消息
    if (responsejs.contains("groups")) {
      // 初始化
      g_currentUserGroupList.clear();

      vector<string> vec1 = responsejs["groups"];
      for (string& groupstr : vec1) {
        json grpjs = json::parse(groupstr);
        Group group;
        group.setId(grpjs["groupid"].get<int>());
        group.setName(grpjs["groupname"]);
        group.setDesc(grpjs["groupdesc"]);

        vector<string> vec2 = grpjs["users"];
        for (string& userstr : vec2) {
          GroupUser user;
          json js = json::parse(userstr);
          user.setId(js["id"].get<int>());
          user.setName(js["name"]);
          user.setRole(js["role"]);
          group.getUsers().push_back(user);
        }
        g_currentUserGroupList.push_back(group);
      }
    }
    // 显示登录用户的基本信息
    showCurrentUserData();

    // 显示当前用户的离线消息
    if (responsejs.contains("offlinemsg")) {
      vector<string> vec = responsejs["offlinemsg"];
      // 接受单独的消息或者群消息
      for (string& str : vec) {
        json js = json::parse(str);
        // time + [id] + name + " said: " + xxx
        if (ONE_CHAT_MSG == js["msgid"].get<int>()) {
          cout << js["time"].get<string>() << " [" << js["id"] << "]"
               << js["name"].get<string>()
               << " said: " << js["msg"].get<string>() << endl;
        } else {
          cout << "群消息：[" << js["groupid"]
               << "]:" << js["time"].get<string>() << " [" << js["id"] << "]"
               << js["name"].get<string>()
               << " said: " << js["msg"].get<string>() << endl;
        }
      }
    }

    g_isLoginSuccess = true;
  }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData() {
  cout << "=========login user=========" << endl;
  cout << "current login user=>id:" << g_currentUser.getId()
       << " name:" << g_currentUser.getName() << endl;
  cout << "-------------friend list---------------" << endl;
  if (!g_currentUserFriendList.empty()) {
    for (User& user : g_currentUserFriendList) {
      cout << user.getId() << " " << user.getName() << " " << user.getState()
           << endl;
    }
  }
  cout << "-------------group list---------------" << endl;
  if (!g_currentUserGroupList.empty()) {
    for (Group& group : g_currentUserGroupList) {
      cout << group.getId() << " " << group.getName() << " " << group.getDesc()
           << endl;
      for (GroupUser& user : group.getUsers()) {
        cout << "\t" << user.getId() << " " << user.getName() << " "
             << user.getState() << " " << user.getRole() << endl;
      }
    }
  }
  cout << "====================================================\n";
}

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},           {"chat", chat},
    {"addfriend", addfriend}, {"creategroup", creategroup},
    {"addgroup", addgroup},   {"groupchat", groupchat},
    {"loginout", loginout}};

// 主聊天页面程序，登录看到的页面
void mainMenu(int clientfd) {
  help();
  char buffer[1024] = {0};
  while (isMainMenuRunning) {
    cin.getline(buffer, 1024);
    string commandbuf(buffer);
    string command;
    int idx = commandbuf.find(":");
    if (idx == -1) {
      command = commandbuf;
    } else {
      command = commandbuf.substr(0, idx);
    }
    auto it = commandHandlerMap.find(command);
    if (it == commandHandlerMap.end()) {
      cerr << "invalid input command!" << endl;
      continue;
    }
    // 调用相应回调函数
    it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
  }
}

// "help" command handler
void help(int, string) {
  cout << "show command list >>>" << endl;
  for (auto& p : commandMap) {
    cout << p.first << " : " << p.second << endl;
  }
  cout << endl;
}

// "addfriend" command handler
void addfriend(int clientfd, string str) {
  int friendid = atoi(str.c_str());
  json js;
  js["msgid"] = ADD_FRIEND_MSG;
  js["id"] = g_currentUser.getId();
  js["friendid"] = friendid;
  string buffer = js.dump();

  if (send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0) == -1) {
    cerr << "send addfriend msg error -> " << buffer << endl;
  }
}

//"chat" command handler
void chat(int clientfd, string str) {
  int idx = str.find(":");
  if (idx == -1) {
    cerr << "chat command invalid!" << endl;
    return;
  }
  int friendid = atoi(str.substr(0, idx).c_str());
  string message = str.substr(idx + 1, str.size() - idx);

  json js;
  js["msgid"] = ONE_CHAT_MSG;
  js["id"] = g_currentUser.getId();
  js["name"] = g_currentUser.getName();
  js["to"] = friendid;
  js["msg"] = message;
  js["time"] = getCurrentTime();
  string buffer = js.dump();

  if (send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0) == -1) {
    cerr << "send chat msg error -> " << buffer << endl;
  }
}

// "creategroup" command handler groupname:groupdesc
void creategroup(int clientfd, string str) {
  int idx = str.find(":");
  if (idx == -1) {
    cerr << "creategroup command invalid!" << endl;
    return;
  }

  string groupname = str.substr(0, idx);
  string groupdesc = str.substr(idx + 1, str.size() - idx);

  json js;
  js["msgid"] = CREATE_GROUP_MSG;
  js["id"] = g_currentUser.getId();
  js["groupname"] = groupname;
  js["groupdesc"] = groupdesc;
  string buffer = js.dump();

  if (send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0) == -1) {
    cerr << "send creategroup msg error -> " << buffer << endl;
  }
}

// "addgroup" command handler
void addgroup(int clientfd, string str) {
  int groupid = atoi(str.c_str());
  json js;
  js["msgid"] = ADD_GROUP_MSG;
  js["id"] = g_currentUser.getId();
  js["groupid"] = groupid;
  string buffer = js.dump();

  if (send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0) == -1) {
    cerr << "send addgroup msg error -> " << buffer << endl;
  }
}

// "groupchat" command handler
void groupchat(int clientfd, string str) {
  int idx = str.find(":");
  if (idx == -1) {
    cerr << "groupchat command invalid!" << endl;
    return;
  }

  int groupid = atoi(str.substr(0, idx).c_str());
  string message = str.substr(idx + 1, str.size() - idx);

  json js;
  js["msgid"] = GROUP_CHAT_MSG;
  js["id"] = g_currentUser.getId();
  js["name"] = g_currentUser.getName();
  js["groupid"] = groupid;
  js["msg"] = message;
  js["time"] = getCurrentTime();
  string buffer = js.dump();

  if (send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0) == -1) {
    cerr << "send groupchat msg error -> " << buffer << endl;
  }
}

// "loginout" command handler
void loginout(int clientfd, string) {
  json js;
  js["msgid"] = LOGINOUT_MSG;
  js["id"] = g_currentUser.getId();
  string buffer = js.dump();

  if (send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0) == -1) {
    cerr << "send loginout msg error -> " << buffer << endl;
  } else {
    isMainMenuRunning = false;
  }
}

// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime() {
  auto tt =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm* ptm = localtime(&tt);
  char date[60] = {0};
  sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900,
          (int)ptm->tm_mon + 1, (int)ptm->tm_mday, (int)ptm->tm_hour,
          (int)ptm->tm_min, (int)ptm->tm_sec);
  return std::string(date);
}