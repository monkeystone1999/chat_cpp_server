#include "server.hpp"
#include <iostream>

int main(int argc, char **argv) {
  if (argc <= 1) {
    std::cerr << "need port Number" << '\n';
    return 0;
  }
  try {
    //    unsigned short port = static_cast<unsigned short>(atoi(argv[1]));
    Server chatServer;
    chatServer.run();
  } catch (const std::exception e) {
    fprintf(stderr, "Error : %s\n", e.what());
  }
}
