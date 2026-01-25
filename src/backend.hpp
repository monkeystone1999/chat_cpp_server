#pragma once

class Backend {
private:
  Backend();

public:
  static Backend &getInstance() {
    static Backend instance;
    return instance;
  }

private:
};
