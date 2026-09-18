#include <cstdint>
#include <iostream>

#include "forgedb/database/database.h"

using namespace forgedb;

int main() {

    // 1. Open or create database
    auto db = Database::open("forgedb_example.db");

    // 2. Create table if it doesn't already exist
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

    // 3. Insert a row
    const auto id = users->insert(
        Tuple{{
            Value{std::int32_t{1}},
            Value{"Suraj"}
        }}
    );

    std::cout << "Inserted RecordId: "
              << id.pageId().value()
              << "\n";

    // 4. Create a query
    Query query;

    query.where({
        0,
        ComparisonOperator::Equal,
        Value{std::int32_t{1}}
    });

    // 5. Execute query
    const auto rows =
        db->select("users", query);

    std::cout << "Rows found: "
              << rows.size()
              << "\n";

    // 6. Close database
    db->close();

    return 0;
}