#include "tuple.h"
#include "transaction_manager.h"
#include <iostream>


using namespace minidb;

void scenario1_visiblity(){
    // Scenario 1: tuple inserted by active transaction is not visible
    // to concurrent transactions (prevents dirty reads).
    // After commit, tuple becomes visible to new snapshots.
    
    TransactionManager tm;
    TransactionId first_xid = tm.begin();

    TupleHeader hdr;
    hdr.xmin = first_xid;
    hdr.xmax = 0;
    hdr.cmin = 0; // assuming first command inseted it
    hdr.infomask = 0;
    hdr.natts = 1;

    // Transaction B begins and takes snapshot BEFORE A commits
    TransactionId second_xid = tm.begin();
    Snapshot snap_b = tm.get_snapshot(second_xid, 0);

    // TODO: assert tuple is NOT visible to B
    assert(tuple_is_visible(hdr, snap_b) == false);
    // TODO: commit A
    tm.commit_transaction(first_xid);
    // TODO: take fresh snapshot as C'

    TransactionId third_xid = tm.begin();
    Snapshot snap_c = tm.get_snapshot(third_xid, 0);


    // TODO: assert tuple IS visible to C
    assert(tuple_is_visible(hdr, snap_c) == true);

}

int main() {
    scenario1_visiblity();
    std::cout << "txn_test: all passed\n";
    return 0;
}