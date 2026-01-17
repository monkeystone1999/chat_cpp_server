#pragma once
#include <algorithm>
#include <arpa/inet.h>
#include <concepts>
#include <cstdlib>
#include <memory.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
template <typename T>
concept SessionLike = requires(T t, int sock, const std::string &ip) {
  { t.OnAccept(sock, ip) }; // 반환 타입 무관 (int 등)
  { t.OnRead(sock) };
};

class Session {
  virtual ~Session() = default;
  virtual int onAccept(int &clientSocket, std::string &info) = 0;
  virtual int onRead() = 0;
};

class Network {
public:
  Network(size_t epoll_size = 100) : epoll_size(epoll_size), epfd(-1) {}
  int addUDP(size_t port);
  int addTCP(size_t port);
  int init();
  template <typename T, typename... Args> int run(T func, Args... args) {
    if (init() < 0)
      return -1;

    struct event_epoll ev_list[epoll_size];
    while (1) {
      int events_on = epoll_wait(epfd, ev_list, 100, 0);
      for (int i = 0; i < events_on; ++i) {
      }
    }
    return func(*this, std::forward<Args>(args)...);
  };

  template <SessionLike T> int run(T &t) {
    if (init() < 0)
      return -1;
    struct epoll_event ev_list[epoll_size];
    while (1) {
      struct sockaddr_in info = {};
      int events_on = epoll_wait(epfd, ev_list, 100, 0);
      for (int i = 0; i < events_on; ++i) {
        auto it = std::find(tcp_fds.begin(), tcp_fds.end(), ev_list[i].data.fd);
        if (it ==
            tcp_fds.end()) { // tcp_fds 에 없다는 소리니 UDP 혹은 ClientFD 이다.
          int clientSockFD =
              accept(ev_list[i].data.fd, (struct sockaddr *)&info,
                     (socklen_t)sizeof(sockaddr_in));
          std::string client_info;
          client_info.resize(INET_ADDRSTRLEN);
          inet_ntop(AF_INET, (struct sockaddr_in *)&info.sin_addr,
                    client_info.data(), INET_ADDRSTRLEN);
          t->Accept(clientSockFD, client_info);
        } else {
          t->newConnect();
        }
      }
    }
  }

private:
  std::vector<int> alloc_sock_tcp(const std::vector<size_t> &ports);
  std::vector<int> alloc_sock_udp(const std::vector<size_t> &ports);
  std::vector<size_t> udp_ports;
  std::vector<size_t> tcp_ports;
  std::vector<int> udp_fds;
  std::vector<int> tcp_fds;
  size_t epoll_size;
  int epfd;
};
