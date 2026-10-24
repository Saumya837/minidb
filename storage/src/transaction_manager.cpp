#include <algorithm>
#include "transaction_manager.h"
#include "snapshot.h"

namespace minidb{

    TransactionId TransactionManager::begin(){
        TransactionId xid = next_xid_.fetch_add(1);

        std::lock_guard<std::mutex> lock(latch_);
        active_xids_.insert(xid);
        xact_status_[xid] = XactStatus::Active;
        return xid;
    }

    void TransactionManager::commit_transaction(TransactionId xid){
        std::lock_guard<std::mutex> lock(latch_);
        xact_status_[xid] = XactStatus::Commited;
        active_xids_.erase(xid);
    }


    void TransactionManager::abort_transaction(TransactionId xid){
        std::lock_guard<std::mutex> lock(latch_);
        xact_status_[xid] = XactStatus::Aborted;
        active_xids_.erase(xid);
    }

    Snapshot TransactionManager::get_snapshot(TransactionId current_xid, CommandId current_cid)
    {
        std::lock_guard<std::mutex> lock(latch_);
        Snapshot snap = Snapshot();
        if(active_xids_.empty()){
            snap.xmin = next_xid_;
            snap.xmax = next_xid_;
        }
        else{
            auto min = std::min_element(active_xids_.begin(), active_xids_.end());
            snap.xmin = *min;
            snap.xmax = next_xid_;
        }
        snap.current_xid = current_xid;
        snap.current_cid = current_cid;
        snap.active_xids = active_xids_;
        return snap;
    }

    XactStatus TransactionManager::get_transaction_status(TransactionId xid){
        std::lock_guard<std::mutex> lock(latch_);
        return xact_status_.at(xid);
    }
}