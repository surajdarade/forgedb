#pragma once
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include "forgedb/record/table.h"
namespace forgedb {
enum class ComparisonOperator { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };
struct Predicate { std::size_t columnIndex; ComparisonOperator operation; Value value; };
class Query {
public:
    Query& where(Predicate predicate);
    Query& limit(std::size_t value);
    [[nodiscard]] const std::vector<Predicate>& predicates() const noexcept;
    [[nodiscard]] std::optional<std::size_t> limitValue() const noexcept;
private:
    std::vector<Predicate> predicates_;
    std::optional<std::size_t> limit_;
};
class QueryEngine {
public:
    static std::vector<Tuple> select(Table& table, const Query& query);
private:
    static bool matches(const Value& lhs, ComparisonOperator op, const Value& rhs);
};
}
