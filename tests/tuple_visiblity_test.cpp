#include "tuple.h"
#include "transaction_manager.h"
#include <iostream>


using namespace minidb;

void test_dirty_read_not_visible(){
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
    assert(tuple_is_visible(hdr, snap_b, tm) == false);

    // TODO: commit A
    tm.commit_transaction(first_xid);

    // TODO: take fresh snapshot as C'
    hdr.infomask = hdr.infomask & ~(HEAP_XMIN_IS_SET) | HEAP_XMIN_COMMITTED;

    TransactionId third_xid = tm.begin();
    Snapshot snap_c = tm.get_snapshot(third_xid, 0);


    // TODO: assert tuple IS visible to C
    assert(tuple_is_visible(hdr, snap_c, tm) == true);
}

void test_same_transaction_delete_not_visible(){
    TransactionManager tm;
    TransactionId first_xid = tm.begin();

    // Insert
    TupleHeader hdr;
    hdr.xmin = first_xid;
    hdr.xmax = XID_INVALID;
    hdr.cmin = 0;
    hdr.infomask = HEAP_XMIN_IS_SET;
    hdr.natts = 1;


    // Same command — not visible yet
    Snapshot snap_a = tm.get_snapshot(first_xid, 0);
    assert(tuple_is_visible(hdr, snap_a, tm) == false);

    // Delete in command 2
    hdr.xmax = first_xid;
    hdr.cmax = 2;
    hdr.infomask = HEAP_XMAX_IS_SET; // clears XMIN_IS_SET, sets XMAX_IS_SET

    // Command 1 — delete hasn't happened yet — visible
    Snapshot snap_b = tm.get_snapshot(first_xid, 1);
    assert(tuple_is_visible(hdr, snap_b, tm) == true);

    // Command 3 — delete has happened — not visible
    Snapshot snap_c = tm.get_snapshot(first_xid, 3);
    assert(tuple_is_visible(hdr, snap_c, tm) == false);
}

void test_transaction_abort_not_visible(){
    //Transaction aborts should not be visible
    TransactionManager tm;
    TransactionId first_xid = tm.begin();
    Snapshot snap = tm.get_snapshot(first_xid, 0);

    TupleHeader hdr_1;
    hdr_1.xmin = first_xid;
    hdr_1.xmax = 0;
    hdr_1.cmin = 0;
    hdr_1.infomask = HEAP_XMIN_IS_SET;
    hdr_1.natts = 1;

    //Transaction is aborted due to some issues
    tm.abort_transaction(first_xid);

    TransactionId second_xid = tm.begin();
    Snapshot snap_b = tm.get_snapshot(second_xid, 0);

    assert(tuple_is_visible(hdr_1, snap_b, tm) == false);

    TupleHeader hdr_2;
    hdr_2.xmin = second_xid;
    hdr_2.xmax = 0;
    hdr_2.cmin = 0;
    hdr_2.infomask = HEAP_XMIN_IS_SET;
    hdr_2.natts = 1;

    //second_transaction commited
    tm.commit_transaction(second_xid);

    TransactionId third_xid = tm.begin();
    hdr_2.xmax = third_xid;
    hdr_2.infomask = hdr_2.infomask | HEAP_XMAX_IS_SET;

    //The third transaction aborted
    tm.abort_transaction(third_xid);//remove from the active list 
    hdr_2.infomask = hdr_2.infomask | HEAP_XMAX_INVALID; //clear the XMAX_IS_SET bit

    TransactionId forth_xid = tm.begin();
    Snapshot snap_d = tm.get_snapshot(forth_xid, 0);
    assert(tuple_is_visible(hdr_2, snap_d, tm) == true);
}

void test_tuple_freeze_visible(){
    TupleHeader hdr_1;
    hdr_1.xmin = XID_FROZEN;
    hdr_1.xmax = 0;
    hdr_1.cmin = 0;
    hdr_1.infomask = HEAP_XMIN_COMMITTED;
    hdr_1.natts = 1;

    TransactionManager tm;
    TransactionId xid = tm.begin();
    Snapshot snap_a = tm.get_snapshot(xid, 0);

    assert(tuple_is_visible(hdr_1, snap_a, tm) == true);
}

void test_committed_delete_not_visible(){
    TransactionManager tm;
    TransactionId a_xid = tm.begin();

    TupleHeader hdr_1;
    hdr_1.xmin = a_xid;
    hdr_1.xmax = 0;
    hdr_1.cmin = 0;
    hdr_1.infomask = HEAP_XMIN_IS_SET;
    hdr_1.natts = 1;

    tm.commit_transaction(a_xid);
    hdr_1.infomask = hdr_1.infomask & ~(HEAP_XMIN_IS_SET) | HEAP_XMIN_COMMITTED;

    TransactionId b_xid = tm.begin();
    hdr_1.xmax = b_xid;
    hdr_1.infomask = hdr_1.infomask | HEAP_XMAX_IS_SET;

    tm.commit_transaction(b_xid);
    hdr_1.infomask = hdr_1.infomask & ~(HEAP_XMAX_IS_SET) | HEAP_XMAX_COMMITTED;

    TransactionId c_xid = tm.begin();
    Snapshot snap_c = tm.get_snapshot(c_xid, 0);

    assert(tuple_is_visible(hdr_1, snap_c, tm) == false);
}



int main() {
    test_dirty_read_not_visible();
    test_same_transaction_delete_not_visible();
    test_transaction_abort_not_visible();
    test_tuple_freeze_visible();
    test_committed_delete_not_visible();
    std::cout << "txn_test: all passed\n";
    return 0;
}