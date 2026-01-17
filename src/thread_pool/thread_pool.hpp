#pragma once
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>
#include <vector>

struct TaskFunc {
  virtual void run() = 0;
  virtual ~TaskFunc() = default;
};

class ThreadPool {
public:
  ThreadPool(size_t threads) : stop(false) {
    event_fd = eventfd(0, EFD_SEMAPHORE);
    if (event_fd == -1) {
      throw std::runtime_error("eventfd 생성 실패");
    }
    for (size_t i = 0; i < threads; ++i) {
      workers.emplace_back(&ThreadPool::worker_thread, this);
    }
  }
  void enqueue(std::unique_ptr<TaskFunc> taskfunc) {
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      tasks.push(std::move(taskfunc));
    }
    uint64_t value = 1;
    // 워커 스레드 깨우기
    ssize_t ret = write(event_fd, &value, sizeof(value));
    if (ret != sizeof(value)) {
      std::cerr << "eventfd write 실패" << std::endl;
    }
  }

  ~ThreadPool() {
    stop = true;
    for (size_t i = 0; i < workers.size();
         ++i) { // 모든 워커들 깨워서 종료시키기
      uint64_t value = 1;
      write(event_fd, &value, sizeof(value));
    }
    for (std::thread &worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    close(event_fd);
  }

private:
  void worker_thread() {
    while (true) {
      uint64_t value;
      ssize_t ret = read(event_fd, &value, sizeof(value)); // 신호 대기
      if (ret != sizeof(value)) {
        std::cerr << "eventfd read 실패" << std::endl;
        continue;
      }
      if (stop) { // 종료 확인
        return;
      }
      std::unique_ptr<TaskFunc> task;
      bool has_task = false;
      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (!tasks.empty()) {
          task = std::move(tasks.front());
          tasks.pop();
          has_task = true;
        }
      }
      if (has_task) {
        task->run();
      }
    }
  }

  std::vector<std::thread> workers;
  std::queue<std::unique_ptr<TaskFunc>> tasks;
  std::mutex queue_mutex;
  std::atomic<bool> stop;
  int event_fd;
};
