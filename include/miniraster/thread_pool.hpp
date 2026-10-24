#pragma once
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mr {
class ThreadPool {
public:
    explicit ThreadPool(std::size_t count);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    void enqueue(std::function<void()> task);
    // Waits for all submitted work; propagates the first worker exception once.
    // Call from the producer thread, never from one of this pool's own tasks.
    void wait();
    std::size_t size() const noexcept { return workers_.size(); }
private:
    void worker();
    void shutdown() noexcept;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable available_, completed_;
    std::size_t active_ = 0;
    bool stopping_ = false;
    std::exception_ptr error_;
};
} // namespace mr
