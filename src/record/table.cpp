#include "forgedb/record/table.h"

#include <stdexcept>
#include <utility>

#include "forgedb/record/tuple_serializer.h"

namespace forgedb {

Table::Table(
    Schema schema,
    HeapFile& heapFile)
    : schema_(std::move(schema)),
      heapFile_(heapFile)
{
}

const Schema& Table::schema() const noexcept
{
    return schema_;
}

RecordId Table::insert(const Tuple& tuple)
{
    if (!tuple.matchesSchema(schema_)) {
        throw std::invalid_argument(
            "Table: tuple does not match schema"
        );
    }

    const auto bytes =
        TupleSerializer::serialize(tuple);

    return heapFile_.insert(bytes);
}

Tuple Table::get(RecordId recordId)
{
    const auto bytes =
        heapFile_.read(recordId);

    Tuple tuple =
        TupleSerializer::deserialize(bytes);

    tuple.setRecordId(recordId);

    if (!tuple.matchesSchema(schema_)) {
        throw std::runtime_error(
            "Table: stored tuple does not match schema"
        );
    }

    return tuple;
}

void Table::update(
    RecordId recordId,
    const Tuple& tuple)
{
    if (!tuple.matchesSchema(schema_)) {
        throw std::invalid_argument(
            "Table: tuple does not match schema"
        );
    }

    const auto bytes =
        TupleSerializer::serialize(tuple);

    heapFile_.update(
        recordId,
        bytes
    );
}

void Table::remove(RecordId recordId)
{
    heapFile_.erase(recordId);
}

} // namespace forgedb
std::vector<forgedb::Tuple> forgedb::Table::scan() {
    std::vector<Tuple> tuples;
    for (const RecordId id : heapFile_.scan()) tuples.push_back(get(id));
    return tuples;
}
