#include "forgedb/query/query.h"

#include <stdexcept>

namespace forgedb {

Query& Query::where(Predicate p) {
    predicates_.push_back(std::move(p));
    return *this;
}

Query& Query::limit(std::size_t v) {
    limit_ = v;
    return *this;
}

const std::vector<Predicate>& Query::predicates() const noexcept {
    return predicates_;
}

std::optional<std::size_t> Query::limitValue() const noexcept {
    return limit_;
}

static int compareValues(const Value& a, const Value& b) {
    if (a.isNull() || b.isNull()) {
        throw std::invalid_argument(
            "Query: NULL comparison requires explicit handling"
        );
    }

    if (a.type() != b.type()) {
        throw std::invalid_argument(
            "Query: incompatible value types"
        );
    }

    if (a == b) {
        return 0;
    }

    switch (a.type()) {
        case DataType::Int32:
            return a.asInt32() < b.asInt32() ? -1 : 1;

        case DataType::Int64:
            return a.asInt64() < b.asInt64() ? -1 : 1;

        case DataType::Float:
            return a.asFloat() < b.asFloat() ? -1 : 1;

        case DataType::Double:
            return a.asDouble() < b.asDouble() ? -1 : 1;

        case DataType::Boolean:
            return a.asBool() < b.asBool() ? -1 : 1;

        case DataType::Varchar:
            return a.asString() < b.asString() ? -1 : 1;
    }

    throw std::logic_error("Query: unknown type");
}

bool QueryEngine::matches(
    const Value& lhs,
    ComparisonOperator op,
    const Value& rhs
) {
    if (lhs.isNull() || rhs.isNull()) {
        return op == ComparisonOperator::Equal &&
               lhs.isNull() &&
               rhs.isNull();
    }

    int c = compareValues(lhs, rhs);

    switch (op) {
        case ComparisonOperator::Equal:
            return c == 0;

        case ComparisonOperator::NotEqual:
            return c != 0;

        case ComparisonOperator::Less:
            return c < 0;

        case ComparisonOperator::LessEqual:
            return c <= 0;

        case ComparisonOperator::Greater:
            return c > 0;

        case ComparisonOperator::GreaterEqual:
            return c >= 0;
    }

    return false;
}

std::vector<Tuple> QueryEngine::select(
    Table& table,
    const Query& q
) {
    std::vector<Tuple> out;

    for (auto tuple : table.scan()) {
        bool ok = true;

        for (const auto& p : q.predicates()) {
            if (p.columnIndex >= tuple.size() ||
                !matches(
                    tuple.getValue(p.columnIndex),
                    p.operation,
                    p.value
                )) {
                ok = false;
                break;
            }
        }

        if (ok) {
            out.push_back(std::move(tuple));

            if (q.limitValue() &&
                out.size() >= *q.limitValue()) {
                break;
            }
        }
    }

    return out;
}

} // namespace forgedb
