#include <cstdint>
#include <iostream>

#include "forgedb/database/database.h"

using namespace forgedb;

int main() {

    // Open or create database.
    auto db = Database::open("students.db");

    // Create table if it does not exist.
    Table* students = nullptr;

    if (db->hasTable("students")) {

        students = &db->openTable("students");

    } else {

        students = &db->createTable(
            "students",
            Schema{{
                Column{"id", DataType::Int32},
                Column{"name", DataType::Varchar, 100},
                Column{"age", DataType::Int32}
            }}
        );
    }

    // Insert rows.
    const auto surajId = students->insert(
        Tuple{{
            Value{std::int32_t{1}},
            Value{"Suraj"},
            Value{std::int32_t{23}}
        }}
    );

    const auto rahulId = students->insert(
        Tuple{{
            Value{std::int32_t{2}},
            Value{"Rahul"},
            Value{std::int32_t{25}}
        }}
    );

    // Build query:
    // id == 1
    Query query;

    query.where({
        0,
        ComparisonOperator::Equal,
        Value{std::int32_t{1}}
    });

    // Execute query.
    const auto rows =
        db->select("students", query);

    std::cout
        << "Rows returned: "
        << rows.size()
        << '\n';

    // Persist buffered pages and close.
    db->close();

    return 0;
}