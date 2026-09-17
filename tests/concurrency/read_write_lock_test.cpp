#include <gtest/gtest.h>
#include "forgedb/concurrency/read_write_lock.h"
namespace forgedb {
TEST(ReadWriteLockTest, SharedAndExclusiveGuardsWork) {
    ReadWriteLock lock;
    { SharedLockGuard guard(lock); SUCCEED(); }
    { ExclusiveLockGuard guard(lock); SUCCEED(); }
}
}
