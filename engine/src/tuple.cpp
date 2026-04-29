#include "types.h"
#include "snapshot.h"
#include "tuple.h"
#include <iostream>

namespace minidb{
    
    bool tuple_is_visible(const TupleHeader& hdr, const Snapshot& snap){
    {
            // 1. invalid tuple
            if (hdr.xmin == XID_INVALID) return false;

            // 2. frozen tuple — always visible
            if (hdr.xmin == XID_FROZEN) return true;

            // 3. same transaction inserted it — check command ID
            if (hdr.xmin == snap.current_xid){
                uint16_t cmin = hdr.infomask & HEAP_XMIN_IS_SET ? hdr.cmin : 0;
                if (cmin >= snap.current_cid)
                    return false;
            } 
            else {
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
                uint16_t cmax = hdr.infomask & HEAP_XMIN_IS_SET ? hdr.cmax : 0;
                if ( hdr.cmax > snap.current_cid){
                    return true;
                }
                return false;
            }

            // 9. deleted by active transaction — still visible to us
            if (snap.active_xids.count(hdr.xmax)) return true;

            // 10. deleted by future transaction — still visible to us
            if (hdr.xmax >= snap.xmax) return true;

            // 11. deleted by committed transaction — not visible
            return false;
        }

    }
}