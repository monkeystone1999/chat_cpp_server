#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;

public:
  ThreadPool(size_t threads);
  ~ThreadPool();
  template <class T, class... Args> void enqueue(T &&t, Args &&...args) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks.emplace(std::bind(std::forward<T>(t), std::forward<Args>(args)...));
    }
    condition.notify_one();
  }
};
