#include "network.hpp"
#include <algorithm>
#include <iostream>

// static 멤버 변수 정의 및 nullptr 초기화
Network* Network::net = nullptr;

int Network::addUDP(size_t port) {
  auto find = std::find(udp_ports.begin(), udp_ports.end(), port);
  if (find != udp_ports.end()) {
    std::cerr << "port reAssign in udp_ports" << '\n' << std::endl;
    return 0;
  }
  find = std::find(tcp_ports.begin(), tcp_ports.end(), port);
  if (find != tcp_ports.end()) {
    std::cerr << "port reAssign in tcp_ports" << '\n' << std::endl;
    return 0;
  }
  udp_ports.emplace_back(port);
  return 0;
}

int Network::addTCP(size_t port) {
  auto find = std::find(udp_ports.begin(), udp_ports.end(), port);
  if (find != udp_ports.end()) {
    std::cerr << "port reAssign in udp_ports" << '\n' << std::endl;
    return 0;
  }
  find = std::find(tcp_ports.begin(), tcp_ports.end(), port);
  if (find != tcp_ports.end()) {
    std::cerr << "port reAssign in tcp_ports" << '\n' << std::endl;
    return 0;
  }
  tcp_ports.emplace_back(port);
  return 0;
}

std::vector<int> Network::alloc_sock_tcp(size_t size) {
  std::vector<int> fd;
  for (int i = 0; i < static_cast<int>(size); ++i) {
    int res = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (res < 0) {
      std::cerr << "TCP Socket create Fail" << std::endl;
      exit(0);
    }
    fd.emplace_back(res);
  }
  return fd;
}
std::vector<int> Network::alloc_sock_udp(size_t size) {
  std::vector<int> fd;
  for (int i = 0; i < static_cast<int>(size); ++i) {
    int res = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (res < 0) {
      std::cerr << "UDP Socket create Fail" << std::endl;
      exit(0);
    }
    fd.emplace_back(res);
  }
  return fd;
}
int Network::init() {
  tcp_fds = alloc_sock_tcp(tcp_ports.size());
  udp_fds = alloc_sock_udp(udp_ports.size());
  epfd = epoll_create1(0);
  struct sockaddr_in server_addr_in;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  int reuse = 1;
  for (int i = 0; i < static_cast<int>(tcp_fds.size()); ++i) {
    memset(&server_addr_in, 0, sizeof(server_addr_in));
    server_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr_in.sin_family = AF_INET;
    server_addr_in.sin_port = htons(tcp_ports[i]);
    setsockopt(tcp_fds[i], SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(tcp_fds[i], (struct sockaddr *)&server_addr_in,
             sizeof(struct sockaddr_in)) == -1) {
      std::cerr << "tcp bind error " << std::endl;
      exit(1);
    }
    if (listen(tcp_fds[i], 5) == -1) {
      std::cerr << "tcp listen fail at " << tcp_fds[i] << std::endl;
      exit(1);
    }
    ev.data.fd = tcp_fds[i];
    epoll_ctl(epfd, EPOLL_CTL_ADD, tcp_fds[i], &ev);
  }
  for (int i = 0; i < static_cast<int>(udp_fds.size()); ++i) {
    memset(&server_addr_in, 0, sizeof(server_addr_in));
    server_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr_in.sin_family = AF_INET;
    server_addr_in.sin_port = htons(udp_ports[i]);
    setsockopt(udp_fds[i], SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(udp_fds[i], (struct sockaddr *)&server_addr_in,
             sizeof(struct sockaddr_in)) == -1) {
      std::cerr << "udp bind error " << std::endl;
      exit(1);
    }
    ev.data.fd = udp_fds[i];
    epoll_ctl(epfd, EPOLL_CTL_ADD, udp_fds[i], &ev);
  }
  return 0;
}

int Network::send(const std::string &msg, int fd) {
  write(fd, msg.data(), msg.size());
  return 0;
}
