#pragma once
#include <cstdint>

namespace minidb{
    using TransactionId = uint32_t;
    static constexpr TransactionId XID_INVALID = 0;
    
}
#pragma once
#include <cstdint>

namespace minidb {

using TransactionId = uint32_t;
static constexpr TransactionId XID_INVALID = 0;
static constexpr TransactionId XID_FROZEN   = 2;

struct TupleHeader {
    TransactionId xmin;  // inserting transaction
    TransactionId xmax;  // deleting transaction (0 if alive)
    uint32_t      cmin;  // command id of insert
    uint32_t      cmax;  // command id of delete
    uint16_t      infomask;  // hint bits
    uint16_t      natts;     // number of attributes
};

struct Snapshot {
    TransactionId xmin;   // lowest active xid
    TransactionId xmax;   // first xid that was not yet started
    // TODO: what else does a snapshot need?
};

// THE hard function
bool tuple_is_visible(const TupleHeader& hdr, const Snapshot& snap);

} // namespace minidb