#pragma once
#include "../inc/crypt.h"
#include "../inc/network.h"
#include "thread_pool/thread_pool.hpp"
#include <map>

struct chat {
  std::string msg;
  std::string user_name;
  std::string time;
};

class Server {
public:
  Server();
  int run();
  void NewConn();
  void RecvMsg();
  void CheckLogin();

private:
  int Backend();
  int onAccept(int sock, const std::string ip);
  int onRead(int sock);
  int onUDP(int data, size_t len, const struct sockaddr_in &addr);
  int onDisconnect(int sock);
  Network *network;
  ThreadPool threadPool;
  std::map<int, std::string> user;
  std::map<std::string, char> chatRoom;
  std::mutex user_mutex, room_mutex;
};

Server::Server() : threadPool(20) {}

int Server::run() {
  network = Network::getInstance(100);
  network->addTCP(60000);
  network->addUDP(60001);
  network->init();
  network->run(*this);
}

int onUDP(int data, size_t len, const struct sockaddr_in &addr) {}
int onDisconnect(int sock) {}

class ipCheck {}
