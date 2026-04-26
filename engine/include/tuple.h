#pragma once
#include <cstdint>  
#include <unordered_set>
#include "snapshot.h"
#include "types.h"
#include <cstdint>
#include <cassert>

namespace minidb {
    struct TupleHeader {
        TransactionId xmin;  // inserting transaction
        TransactionId xmax;  // deleting transaction (0 if alive)
        union {
            uint32_t cmin;
            uint32_t cmax;
        };
        uint16_t      infomask;  // hint bits
        uint16_t      natts;     // number of attributes
    };

    static_assert(sizeof(TupleHeader) == 16, "TupleHeader size must be 16 bytes");

    bool tuple_is_visible(const TupleHeader& hdr, const Snapshot& snap);

} // namespace minidb