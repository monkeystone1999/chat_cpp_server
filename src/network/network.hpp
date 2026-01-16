#pragma once
#include <arpa/inet.h>
#include <cstdlib>
#include <memory.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
class Network {
public:
  Network(size_t epoll_size = 100) : epoll_size(epoll_size), epfd(-1) {}
  int addUDP(size_t port);
  int addTCP(size_t port);
  int init();
  template <typename T, typename... Args> int run(T func, Args... args) {
    if (init() < 0)
      return -1;
    return func(*this, std::forward<Args>(args)...);
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
