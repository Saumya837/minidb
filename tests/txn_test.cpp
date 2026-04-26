#include <iostream>
#include <cassert>
#include "transaction_manager.h"
#include "tuple.h"

using namespace minidb;

void test_basic_lifecycle() {
    TransactionManager tm;

    // TODO: begin a transaction, assert XID is 3
    TransactionId first_xid = tm.begin();
    assert(first_xid == 3);

    vector<TransactionId> active_xids;
    for(int i = 0; i<5; i++){
        TransactionId temp_xid = tm.begin();
        active_xids.emplace_back(temp_xid);
    }

    // TODO: get a snapshot
    Snapshot snap = tm.get_snapshot(first_xid, 0);
    Snapshot snap_5 = tm.get_snapshot(active_xids[4], 0);

    assert(snap_5.xmin == 3);
    assert(snap_5.xmax == 9);

    // TODO: assert the transaction is in active_xids of snapshot
    assert(snap_5.active_xids.count(first_xid) != 0);
    
    // TODO: commit the transaction
    tm.commit_transaction(first_xid);

    // TODO: assert status is Committed
    assert(tm.get_transaction_status(first_xid) == XactStatus::Commited);

     //TODO: assert status is Aborted
    tm.abort_transaction(active_xids[1]);

    // TODO: assert status is Aborted
    assert(tm.get_transaction_status(active_xids[1]) == XactStatus::Aborted);

    TransactionId next_xid = tm.begin();
    Snapshot snap_next_xid = tm.get_snapshot(next_xid, 0);

    // TODO: assert commited xid no longer in active set
    assert(snap_next_xid.active_xids.count(first_xid) == 0);

    // TODO: assert aborted xid longer in active set
     assert(snap_next_xid.active_xids.count(active_xids[1]) == 0);

}

int main() {
    test_basic_lifecycle();
    std::cout << "txn_test: all passed\n";
    return 0;
}