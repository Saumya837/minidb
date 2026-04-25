#pragma once
#include <cstdint>
#include <unordered_set>
#include "types.h"

namespace minidb{

    struct Snapshot {
        TransactionId xmin;   // lowest active xid
        TransactionId xmax;   // first xid that was not yet started
        TransactionId current_xid;  // who took this snapshot
        uint32_t      current_cid;  // current command ID
        std::unordered_set<TransactionId> active_xids; // set of active transactions at the time of snapshot
    };
}
