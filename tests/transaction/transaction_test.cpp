#include <gtest/gtest.h>
#include "forgedb/transaction/transaction.h"
namespace forgedb {
TEST(TransactionTest, AbortRunsUndoInReverseOrder) {
    TransactionManager manager;
    auto tx = manager.begin();
    std::vector<int> order;
    tx->addUndo([&]{ order.push_back(1); });
    tx->addUndo([&]{ order.push_back(2); });
    manager.abort(tx);
    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(tx->state(), TransactionState::Aborted);
}
}
