#include "forgedb/concurrency/read_write_lock.h"

namespace forgedb {
void ReadWriteLock::lockShared() { std::unique_lock lock(mutex_); condition_.wait(lock, [&]{ return !writerActive_ && waitingWriters_ == 0; }); ++readers_; }
void ReadWriteLock::unlockShared() { std::lock_guard lock(mutex_); if (readers_ == 0) return; --readers_; if (readers_ == 0) condition_.notify_all(); }
void ReadWriteLock::lockExclusive() { std::unique_lock lock(mutex_); ++waitingWriters_; condition_.wait(lock, [&]{ return !writerActive_ && readers_ == 0; }); --waitingWriters_; writerActive_=true; }
void ReadWriteLock::unlockExclusive() { std::lock_guard lock(mutex_); writerActive_=false; condition_.notify_all(); }
} // namespace forgedb
