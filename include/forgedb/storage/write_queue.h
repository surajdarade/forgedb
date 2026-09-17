#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>

namespace forgedb {

class WriteQueue {
public:
    explicit WriteQueue(std::size_t workerCount = 1);
    ~WriteQueue();
    WriteQueue(const WriteQueue&) = delete;
    WriteQueue& operator=(const WriteQueue&) = delete;
    void enqueue(std::function<void()> operation);
    void drain();
    void shutdown();
private:
    void workerLoop();
    std::mutex mutex_;
    std::condition_variable condition_;
    std::condition_variable drained_;
    std::queue<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    std::size_t active_{0};
    bool stopping_{false};
};
} // namespace forgedb
