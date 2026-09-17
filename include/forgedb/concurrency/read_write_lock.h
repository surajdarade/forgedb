#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace forgedb {

class ReadWriteLock {
public:
    void lockShared();
    void unlockShared();
    void lockExclusive();
    void unlockExclusive();

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::size_t readers_{0};
    std::size_t waitingWriters_{0};
    bool writerActive_{false};
};

class SharedLockGuard {
public:
    explicit SharedLockGuard(ReadWriteLock& lock) : lock_(lock) { lock_.lockShared(); }
    ~SharedLockGuard() { lock_.unlockShared(); }
    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;
private:
    ReadWriteLock& lock_;
};

class ExclusiveLockGuard {
public:
    explicit ExclusiveLockGuard(ReadWriteLock& lock) : lock_(lock) { lock_.lockExclusive(); }
    ~ExclusiveLockGuard() { lock_.unlockExclusive(); }
    ExclusiveLockGuard(const ExclusiveLockGuard&) = delete;
    ExclusiveLockGuard& operator=(const ExclusiveLockGuard&) = delete;
private:
    ReadWriteLock& lock_;
};

} // namespace forgedb
