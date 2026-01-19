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

template <typename T>
concept Task = requires(T t) {
  { t.ThrowWork() } -> std::same_as<void>;
};

struct TaskFunc {
  virtual void run() = 0;
  virtual ~TaskFunc() = default;
};

class ThreadPool {
public:
  // 생성자: 스레드 풀을 초기화하고 지정된 수만큼의 워커 스레드를 생성하여 대기
  // 상태로 만듭니다.
  ThreadPool(size_t threads) : stop(false) {
    event_fd = eventfd(0, EFD_SEMAPHORE);
    if (event_fd == -1) {
      throw std::runtime_error("eventfd 생성 실패");
    }
    for (size_t i = 0; i < threads; ++i) {
      workers.emplace_back(&ThreadPool::worker_thread, this);
    }
  }
  // 작업을 스레드 풀의 작업 큐에 등록합니다.
  // 인자로 전달된 Task 객체는 std::unique_ptr로 관리되며, 소유권이 이동됩니다.
  // 작업 등록 후에는 대기 중인 워커 스레드 중 하나를 깨워 작업을 처리하게
  // 합니다. usage : std::unique_ptr<Task> someFunc =
  // std::make_unique<custom>(); threadPool.enqueue(std::move(someFunc))
  template <Task T> void enqueue(std::unique_ptr<T> ThrowWork) {
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      tasks.push([t = std::move(ThrowWork)]() mutable { t->ThrowWork(); });
    }
    uint64_t value = 1;
    // 워커 스레드 깨우기
    ssize_t ret = write(event_fd, &value, sizeof(value));
    if (ret != sizeof(value)) {
      std::cerr << "eventfd write 실패" << std::endl;
    }
  }
  template <typename T, typename... Args> void enqueue(T &&t, Args &&...args) {
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      tasks.push(std::bind(std::forward<T>(t), std::forward<Args>(args)...));
    }
    uint64_t value = 1;
    ssize_t ret = write(event_fd, &value, sizeof(value));
    if (ret != sizeof(value)) {
      std::cerr << "eventfd write 실패" << std::endl;
    }
  }

  // 소멸자: 스레드 풀 운영을 중단하고 모든 워커 스레드를 안전하게 종료합니다.
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
  // 워커 스레드가 실행하는 루프 함수입니다.
  // 작업 큐에 작업이 들어올 때까지 대기(sleep)하다가, 작업이 들어오면
  // 깨어나서 처리를 수행합니다.
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
      std::function<void()> task;
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
        task();
      }
    }
  }

  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queue_mutex;
  std::atomic<bool> stop;
  int event_fd;
};
