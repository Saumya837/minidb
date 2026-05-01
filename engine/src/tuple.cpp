#include "types.h"
#include "snapshot.h"
#include "tuple.h"
#include <iostream>

namespace minidb {
    bool tuple_is_visible(const TupleHeader& hdr, const Snapshot& snap){

        //1. invalid tuple
        if (hdr.xmin == XID_INVALID){
            return false;
        } 

        //2. Frozen Tuple
        if(hdr.xmin == XID_FROZEN){
            return true;
        } 

        //3. Current Trnsaction 
        if(hdr.xmin == snap.current_xid) {
            int16_t cmin = hdr.infomask & HEAP_XMIN_IS_SET ? hdr.cmin : 0;
            if(cmin >= snap.current_cid) return false;
        }
        else{
                //xmin checks
                //4. Inserted by a future transaction - not visible
                if(hdr.xmin >=  snap.xmax) return false;
                    
                //5. Inserted by a active transaction - not committed - not visible
                else if(snap.active_xids.count(hdr.xmin) != 0) return false;
                
                    
                //6. Inseted by an older transaction but invalid
                else if (hdr.infomask == HEAP_NONE) return false;
        } 

        

        //xmax checks
        //7. Not deleted by anybody, Xmax not set 
        if(hdr.xmax == XID_INVALID) return true;

        //8. Not deleted by anybody, Xmax not set 
        if(hdr.xmax == snap.current_xid){
            uint16_t cmax = hdr.infomask & HEAP_XMAX_IS_SET ? hdr.cmax : 0; 
            if(cmax > snap.current_cid) return true;
            return false;
        }

        //9.check if xmax is asborted - tuple is alive
        if(HEAP_XMAX_INVALID & hdr.infomask) return true;
 
        // xmax known committed — tuple is dead
        if (hdr.infomask & HEAP_XMAX_COMMITTED) return false;

        //deleted by a future transaction 
        if(hdr.xmax >= snap.xmax) return true;

        //deleted by an active transaction
        if(snap.active_xids.count(hdr.xmax) != 0)
            return true;

        // should never reach here
        assert(false && "tuple_is_visible: unhandled case");    
    }
};