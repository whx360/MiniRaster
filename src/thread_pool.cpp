#include "miniraster/thread_pool.hpp"
#include <stdexcept>

namespace mr {
ThreadPool::ThreadPool(std::size_t count) {
    if (count == 0) throw std::invalid_argument("thread count must be positive");
    try {
        workers_.reserve(count);
        for (std::size_t i=0; i<count; ++i) workers_.emplace_back([this] { worker(); });
    } catch (...) { shutdown(); throw; }
}
ThreadPool::~ThreadPool() { shutdown(); }
void ThreadPool::shutdown() noexcept {
    { std::lock_guard<std::mutex> lock(mutex_); stopping_=true; }
    available_.notify_all();
    for (auto& worker_thread : workers_) if (worker_thread.joinable()) worker_thread.join();
}
void ThreadPool::enqueue(std::function<void()> task) {
    if (!task) throw std::invalid_argument("cannot enqueue empty task");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) throw std::runtime_error("thread pool is stopping");
        tasks_.push(std::move(task));
    }
    available_.notify_one();
}
void ThreadPool::wait() {
    std::exception_ptr error;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        completed_.wait(lock,[this] { return tasks_.empty() && active_==0; });
        error = error_; error_=nullptr;
    }
    if (error) std::rethrow_exception(error);
}
void ThreadPool::worker() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            available_.wait(lock,[this] { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) return;
            task=std::move(tasks_.front()); tasks_.pop(); ++active_;
        }
        try { task(); }
        catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!error_) error_=std::current_exception();
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --active_;
            if (tasks_.empty() && active_==0) completed_.notify_all();
        }
    }
}
} // namespace mr
