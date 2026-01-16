#pragma once
#include "../inc/crypt.h"
#include "../inc/network.h"
#include "thread_pool/thread_pool.hpp"

class Server {
public:
  Server();
  int run();

private:
  int Backend();
  Network network;
  ThreadPool<> threadPool;
};

int Server::run() { network.run(Backend); }
