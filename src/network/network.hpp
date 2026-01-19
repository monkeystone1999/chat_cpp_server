#pragma once
#include <algorithm>
#include <arpa/inet.h>
#include <concepts>
#include <cstdlib>
#include <iostream>
#include <memory.h>
#include <netinet/in.h>
#include <set>
#include <stdlib.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// SessionManager concept
// onAccept: TCP 클라이언트 접속 시 호출 (로그인/등록용)
// onRead: TCP 클라이언트 데이터 수신 시 호출
// onUDP: UDP 데이터 수신 시 호출 (채팅 메시지용)
// onDisconnect: 클라이언트 연결 종료 시 호출
template <typename T>
concept SessionManager =
    requires(T t, int sock, const std::string &ip, const char *data, size_t len,
             const struct sockaddr_in &addr) {
      { t.onAccept(sock, ip) };     // TCP 연결 SSL 안 씀
      { t.newConnect(sock, ip) };   // TCP 연결 SSL 안 씀
      { t.onRead(sock) };           // TCP 연결 로그인임
      { t.onUDP(data, len, addr) }; // 채팅 메시지 받는 곳
      { t.onDisconnect(sock) };     // 연결 끊긔
    };

class Session {
  virtual ~Session() = default;
  virtual int onAccept(int &clientSocket, std::string &info) = 0;
  virtual int onRead(int sock) = 0;
};

class Network {
  Network(size_t epoll_size = 100) : epoll_size(epoll_size), epfd(-1) {}

public:
  static Network *getInstance(size_t epoll_size = 100) {
    if (net == nullptr) {
      net = new Network(epoll_size);
    }
    return net;
  }
  int send(const std::string &msg, int fd);
  int sendUDP(const std::string &msg, const struct sockaddr_in &addr);
  int addUDP(size_t port);
  int addTCP(size_t port);
  int init();
  // 클라이언트 FD를 epoll에서 제거 (연결 종료 시 호출)
  void removeClient(int clientFd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, clientFd, nullptr);
    client_fds.erase(clientFd);
    close(clientFd);
  }
  template <SessionManager T> int run(T &t) {
    if (init() < 0)
      return -1;
    std::vector<struct epoll_event> ev_list(epoll_size);
    while (true) {
      int events_on = epoll_wait(epfd, ev_list.data(), epoll_size, -1);
      if (events_on < 0) {
        std::cerr << "epoll_wait error" << std::endl;
        continue;
      }
      for (int i = 0; i < events_on; ++i) {
        int fd = ev_list[i].data.fd;
        // 1. TCP 서버 소켓인지 확인 (새 연결 accept)
        auto tcp_it = std::find(tcp_fds.begin(), tcp_fds.end(), fd);
        if (tcp_it != tcp_fds.end()) {
          // TCP 서버 소켓 → 새 클라이언트 연결
          struct sockaddr_in client_addr = {};
          socklen_t addr_len = sizeof(client_addr);
          int clientFd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
          if (clientFd < 0) {
            std::cerr << "accept error" << std::endl;
            continue;
          }
          // 클라이언트 IP 추출
          char ip_str[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
          std::string client_ip(ip_str);
          // 클라이언트 소켓을 epoll에 등록
          struct epoll_event ev;
          ev.events = EPOLLIN;
          ev.data.fd = clientFd;
          epoll_ctl(epfd, EPOLL_CTL_ADD, clientFd, &ev);
          client_fds.insert(clientFd);
          // SessionManager 콜백 호출
          t.onAccept(clientFd, client_ip);
          continue;
        }
        // 2. UDP 소켓인지 확인 (채팅 메시지 수신)
        auto udp_it = std::find(udp_fds.begin(), udp_fds.end(), fd);
        if (udp_it != udp_fds.end()) {
          // UDP 소켓 → 데이터 수신
          char buffer[4096];
          struct sockaddr_in sender_addr = {};
          socklen_t addr_len = sizeof(sender_addr);
          ssize_t recv_len =
              recvfrom(fd, buffer, sizeof(buffer) - 1, 0,
                       (struct sockaddr *)&sender_addr, &addr_len);
          if (recv_len > 0) {
            buffer[recv_len] = '\0';
            t.onUDP(buffer, recv_len, sender_addr);
          }
          continue;
        }
        // 3. 클라이언트 TCP 소켓 (로그인/데이터 수신)
        if (client_fds.find(fd) != client_fds.end()) {
          // 연결 종료 또는 에러 체크
          if (ev_list[i].events & (EPOLLHUP | EPOLLERR)) {
            t.onDisconnect(fd);
            removeClient(fd);
            continue;
          }
          // 데이터 수신 가능
          if (ev_list[i].events & EPOLLIN) {
            // peek로 연결 상태 확인
            char peek_buf;
            ssize_t peek_ret = recv(fd, &peek_buf, 1, MSG_PEEK);
            if (peek_ret <= 0) {
              // 연결 종료
              t.onDisconnect(fd);
              removeClient(fd);
              continue;
            }
            // SessionManager 콜백 호출 (실제 read는 Session에서 수행)
            t.onRead(fd);
          }
        }
      }
    }
    return 0;
  }

private:
  static Network *net;
  std::vector<int> alloc_sock_tcp(size_t size);
  std::vector<int> alloc_sock_udp(size_t size);
  std::vector<size_t> udp_ports;
  std::vector<size_t> tcp_ports;
  std::vector<int> udp_fds;
  std::vector<int> tcp_fds;
  std::set<int> client_fds; // 연결된 클라이언트 FD 추적
  size_t epoll_size;
  int epfd;
};
