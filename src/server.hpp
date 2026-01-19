#pragma once
#include "../inc/crypt.h"
#include "../inc/network.h"
#include "../inc/thread_pool.h"
#include <map>
#include <mutex>

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
  bool newConnect(int sock, const std::string &ip);
  int onTCP(Network *net, int sock);
  int onUDP(Network *net, int sock);
  int offTCP(Network *net, int sock);

private:
  int Backend();
  Network *network;
  ThreadPool threadPool;
  std::map<int, std::string> user;
  std::mutex user_mutex, room_mutex;
  std::map<std::string, chat> ChatRoom;
};

Server::Server() : threadPool(20) {}

int Server::run() {
  network = Network::getInstance(100);
  network->addTCP(60000);
  network->addUDP(60001);
  network->init();
  return network->run(*this);
}

/// 로그인시 onTCP
int Server::onTCP(Network *net, int sock) {
  /// SSL 연결 후
  /// 해당 IP 가 접속을 했었는지 확인
  /// 해당 로그인 정보가 정확한지 DB에 확인
  /// 결과 리턴
}
/// 메시지를 받는 곳
int Server::onUDP(Network *net, int sock) {
  /// DTLP 를 사용해서 받음
  /// 해당 IP 가 존재했는지 확인 로그인 정보를 확인을 하여 로그인 하지 않았다면
  /// 튕겨냄 채팅방 정보까지 맞다면 메시지를 받음 그리고 채팅방의 사용자들에게
  /// 메시지를 다 뿌림
}
/// TCP 연결 끊는건데 언제 끊는건지는 몰?루
int Server::offTCP(Network *net, int sock) {}
/// 새로운 연결이 들어올 때
bool Server::newConnect(int sock, const std::string &ip) {
  /// SSL 연결로 새로운 연결을 받기는 한다
  /// ip 를 특정한 위치에 저장을 함
  /// 인증서로 확인을 함
  return false;
}
