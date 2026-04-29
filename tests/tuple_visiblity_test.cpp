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
    hdr.infomask = HEAP_XMIN_IS_SET;
    hdr.natts = 1;

    // Transaction B begins and takes snapshot BEFORE A commits
    TransactionId second_xid = tm.begin();
    Snapshot snap_b = tm.get_snapshot(second_xid, 0);

    // TODO: assert tuple is NOT visible to B
    assert(tuple_is_visible(hdr, snap_b) == false);

    // TODO: commit A
    tm.commit_transaction(first_xid);

    // TODO: take fresh snapshot as C'
    hdr.infomask = HEAP_XMIN_COMMITTED;

    TransactionId third_xid = tm.begin();
    Snapshot snap_c = tm.get_snapshot(third_xid, 0);


    // TODO: assert tuple IS visible to C
    assert(tuple_is_visible(hdr, snap_c) == true);

}

void scenario2_visiblity(){
    // The transaction deletes it's own insertion 

    TransactionManager tm;
    TransactionId first_xid = tm.begin();

    TransactionId second_xid = tm.begin();
    Snapshot snap_a = tm.get_snapshot(second_xid, 0);

    //Insertion by the 2nd transaction
    TupleHeader hdr;
    hdr.xmin = second_xid;
    hdr.xmax = 0;
    hdr.cmin = 0; 
    hdr.infomask = HEAP_XMIN_IS_SET;
    hdr.natts = 1;

    assert(tuple_is_visible(hdr, snap_a) == false);

    //Deletion by the same transaction 
    hdr.xmax = second_xid;
    hdr.cmax = 2;
    hdr.infomask = HEAP_XMAX_IS_SET;

    Snapshot snap_b = tm.get_snapshot(second_xid, 1);
    assert(tuple_is_visible(hdr, snap_b) == true); 

    Snapshot snap_c = tm.get_snapshot(second_xid, 3);
    assert(tuple_is_visible(hdr, snap_c) == false);
}

int main() {
    scenario1_visiblity();
    scenario2_visiblity();
    std::cout << "txn_test: all passed\n";
    return 0;
}