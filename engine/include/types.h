#pragma once
#include <cstdint>

namespace minidb {
    using TransactionId = uint32_t;
    using CommandId     = uint32_t;
    using OID           = uint32_t;

    static constexpr TransactionId XID_INVALID = 0;
    static constexpr TransactionId XID_FROZEN  = 2;
}