#pragma once
#include <cstdint>  
#include <unordered_set>
#include "snapshot.h"
#include "types.h"
#include <cstdint>
#include <cassert>
namespace minidb {

    static constexpr uint16_t HEAP_NONE           = 0x0000;
    static constexpr uint16_t HEAP_XMIN_IS_SET    = 0x0001;
    static constexpr uint16_t HEAP_XMAX_IS_SET    = 0x0002;
    static constexpr uint16_t HEAP_XMIN_COMMITTED = 0x0100;
    static constexpr uint16_t HEAP_XMIN_INVALID   = 0x0200;
    static constexpr uint16_t HEAP_XMAX_COMMITTED = 0x0400;
    static constexpr uint16_t HEAP_XMAX_INVALID   = 0x0800;
    static constexpr uint16_t HEAP_XMAX_IS_MULTI  = 0x1000; 

    struct TupleHeader {
        TransactionId xmin;  // inserting transaction
        TransactionId xmax;  // deleting transaction (0 if alive)
        union {
            uint32_t cmin;
            uint32_t cmax;
        };
        uint16_t    infomask;  // hint bits
        uint16_t      natts;     // number of attributes
    };

    static_assert(sizeof(TupleHeader) == 16, "TupleHeader size must be 16 bytes");

    bool tuple_is_visible(const TupleHeader& hdr, const Snapshot& snap);

} // namespace minidb