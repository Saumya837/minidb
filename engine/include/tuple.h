#pragma once
#include <cstdint>  
#include <unordered_set>
#include <snapshot.h>
#include "types.h"

using namespace std;

#pragma once
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

bool tuple_is_visible(const TupleHeader& hdr, const Snapshot& snap) {

    // 1. invalid tuple
    if (hdr.xmin == XID_INVALID) return false;

    // 2. frozen tuple — always visible
    if (hdr.xmin == XID_FROZEN) return true;

    // 3. same transaction inserted it — check command ID
    if (hdr.xmin == snap.current_xid) {
        if (hdr.cmin >= snap.current_cid) return false;
        // inserted by earlier command — fall through to xmax checks
    } else {
        // 4. inserted by future transaction — not visible
        if (hdr.xmin >= snap.xmax) return false;

        // 5. inserted by active transaction — not visible
        if (snap.active_xids.count(hdr.xmin)) return false;

        // 6. inserted by committed transaction — fall through to xmax checks
    }

    // --- xmax checks ---

    // 7. not deleted — visible
    if (hdr.xmax == XID_INVALID) return true;

    // 8. same transaction deleted it — check command ID
    if (hdr.xmax == snap.current_xid) {
        if (hdr.cmax > snap.current_cid) return true;
        return false;
    }

    // 9. deleted by active transaction — still visible to us
    if (snap.active_xids.count(hdr.xmax)) return true;

    // 10. deleted by future transaction — still visible to us
    if (hdr.xmax >= snap.xmax) return true;

    // 11. deleted by committed transaction — not visible
    return false;
}

} // namespace minidb