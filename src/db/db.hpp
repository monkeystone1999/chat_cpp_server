#pragma once
#include <functional>
#include <iostream>
#include <libpq-fe.h>
#include <memory>

namespace DB {

class postgreDB {
  PostgreDB();
  PGconn *conn = nullptr;

public:
  static PGconn *getInstance() {
    if (conn == nullptr) {
      return nullptr;
    }
    return conn;
  }
  static postgreDB &getInstance() {
    static postgreDB instance;
    return instance;
  }
  ~PostgreDB() {}

private:
}

template <typename T>
class Table {
private:
  Table();
  std::vector<T> row;
  std::mutex m;

public:
  static Table<T> &getInstance() {
    static Table<T> instance;
    return instance;
  }
  std::vector<T> find(std::function<bool(const T &)> predicate) {
    std::lock_guard<std::mutex> lock(m);
    std::vector<T> arr;
    for (const auto &r : row) {
      if (predicate(r)) {
        arr.push_back(r);
      }
    }
    return arr;
  }
};
} // namespace DB

/// DB(userTable<userInfo>.find([](const userInfo info){
/// return info.username == "Kim";
/// }))
/// DB(userTable<userInfo>.)
struct userInfo {};
// class DB {
// private:
//   DB();

// public:
//   static DB &getInstance() {
//     static DB instance;
//     return instance;
//   }

//};
//} // namespace DataStore
