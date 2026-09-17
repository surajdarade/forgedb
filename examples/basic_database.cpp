#include <cstdint>
#include "forgedb/database/database.h"

using namespace forgedb;

int main() {
    auto db = Database::open("forgedb_example.db");

    Table* users = nullptr;
    if (db->hasTable("users")) {
        users = &db->openTable("users");
    } else {
        users = &db->createTable(
            "users",
            Schema{{
                Column{"id", DataType::Int32},
                Column{"name", DataType::Varchar, 100}
            }}
        );
    }

    const auto insertedId = users->insert(Tuple{{
        Value{std::int32_t{1}},
        Value{"Suraj"}
    }});
    (void)insertedId;

    Query query;
    query.where({
        0,
        ComparisonOperator::Equal,
        Value{std::int32_t{1}}
    });

    const auto rows = db->select("users", query);
    (void)rows;

    db->close();
}
