#include <signal.h>

#include <iostream>

#include "chatserver.h"
#include "chatservice.h"
#include "db.hpp"
using namespace std;
void resetHandler(int) {
  ChatService::Instance()->reset();
  exit(0);
}
int main(int argc, char* argv[]) {
  if (argc < 3) {
    cerr << "command invalid! example:./ChatServer 127.0.0.1 6000";
  }
  signal(SIGINT, resetHandler);
  EventLoop loop;
  InetAddress addr(argv[1], atoi(argv[2]));
  ChatServer chatserver(&loop, addr, "chatserver");
  chatserver.Start();
  loop.loop();
  return 0;
}