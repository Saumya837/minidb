#include <iostream>
#include "lock_manager.h"
#include <cassert>
#include <thread>

using namespace minidb;

void access_share_lock_test(){
    /*
        AccessShare lock only conflcts with AccessExclusiveLock,
        so we should be able to acquire AccessShare, RowShare, RowExclusive,
        ShareUpodateExclusive, Share, ShareRowExclusive, Exclusivelocks Without blocking
        
        The test will acquire AccessShare lock first, 
        then try to acquire other locks and assert that they are granted immediately without blocking.
    */
    LockManager lm;
    LockTag tag;
    tag.type = LockTagType::Tuple;
    tag.relnumber = 1;
    tag.block_num = 2;
    tag.offset = 3;

    // acquire AccessShare lock as xid=3
    bool status_3 = lm.acquire_lock(3, tag, LockMode::AccessShare);

    // acquire AccessShare lock as xid=4
    bool status_4 = lm.acquire_lock(4, tag, LockMode::AccessShare);
    assert(status_3 == true);
    assert(status_4 == true);

    //acquire RowExclusive lock as xid=5 — should allow
    bool status_5 = lm.acquire_lock(5, tag, LockMode::RowShare);
    assert(status_5 == true);
    lm.release_lock(5, tag);

    //acquire RowExclusive lock as xid=6 — should allow
    bool status_6 = lm.acquire_lock(6, tag, LockMode::RowExclusive);
    assert(status_6 == true);
    lm.release_lock(6, tag);

    //acquire Exclusive lock as xid = 7 — should allow
    bool status_7 = lm.acquire_lock(7, tag, LockMode::ShareUpdateExclusive);
    assert(status_7 == true);
    lm.release_lock(7, tag);

    //acquire Exclusive lock as xid = 8 — should allow
    bool status_8 = lm.acquire_lock(8, tag, LockMode::Share);
    assert(status_8 == true);
    lm.release_lock(8, tag);

    //acquire Exclusive lock as xid = 9 — should allow
    bool status_9 = lm.acquire_lock(9, tag, LockMode::ShareRowExclusive);
    assert(status_9 == true);
    lm.release_lock(9, tag);

    //acquire ShareRowExclusive lock as xid = 10 — should allow
    bool status_10 = lm.acquire_lock(10, tag, LockMode::Exclusive);
    assert(status_10 == true);
    lm.release_lock(10, tag);

    std::cout << "Share lock test passed\n";
}

void RowShare_lock_test(){
    /*
        RowShare lock only conflcts with AccessExclusiveLock, Exclusive
        so we should be able to acquire AccessShare, RowShare, RowExclusive,
        ShareUpodateExclusive, Share, ShareRowExclusive, Exclusivelocks Without blocking
            
        The test will acquire RowShare lock first,
        then try to acquire other locks and assert that they are granted immediately without blocking.
    */

    LockManager lm;
    LockTag tag;
    tag.type = LockTagType::Tuple;
    tag.relnumber = 1;
    tag.block_num = 2;
    tag.offset = 3;

    // acquire AccessShare lock as xid = 3
    bool status_3 = lm.acquire_lock(3, tag, LockMode::RowShare);

    //acquire RowExclusive lock as xid = 4 — should allow
    bool status_4 = lm.acquire_lock(4, tag, LockMode::RowExclusive);
    assert(status_3 == true);
    assert(status_4 == true);
    lm.release_lock(4, tag);

    //acquire RowExclusive lock as xid = 5 — should allow
    bool status_5 = lm.acquire_lock(5, tag, LockMode::ShareUpdateExclusive);
    assert(status_5 == true);
    lm.release_lock(5, tag);

    //acquire Exclusive lock as xid = 6 — should allow
    bool status_6 = lm.acquire_lock(6, tag, LockMode::Share);
    assert(status_6 == true);
    lm.release_lock(6, tag);

    //acquire Exclusive lock as xid = 7 — should allow
    bool status_7 = lm.acquire_lock(7, tag, LockMode::ShareRowExclusive);
    assert(status_7 == true);
    lm.release_lock(7, tag);

    std::cout << "RowShare lock test passed\n";
}

void RowExclusiveShare_lock_test()
{
    /*
        RowExclusive lock only conflcts with AccessExclusiveLock, Share, ShareRowExclusive, Exclusive
        so we should be able to acquire AccessShare, RowShare, RowExclusive,
        ShareUpodateExclusive locks Without blocking
            
        The test will acquire RowExclusive lock first, 
        then try to acquire other locks and assert that they are granted immediately without blocking.
    */

    LockManager lm;
    LockTag tag;
    tag.type = LockTagType::Tuple;
    tag.relnumber = 1;
    tag.block_num = 2;
    tag.offset = 3;

    //acquire RowExclusive lock as xid = 4 — should allow
    bool status_4 = lm.acquire_lock(4, tag, LockMode::RowExclusive);

    //acquire RowExclusive lock as xid = 5 — should allow
    bool status_5 = lm.acquire_lock(5, tag, LockMode::RowExclusive);
    assert(status_4 == true);
    assert(status_5 == true);

    std::cout << "RowExclusiveShare lock test passed\n";
}


void test_blocking_lock() {
    LockManager lm;
    LockTag tag;
    tag.type = LockTagType::Tuple;
    tag.relnumber = 1;
    tag.block_num = 0;
    tag.offset = 5;

    // thread 1 acquires AccessExclusive
    lm.acquire_lock(3, tag, LockMode::AccessExclusive);

    // thread 2 tries AccessShare — should block
    bool thread2_granted = false;
    std::thread t2([&]() {
        lm.acquire_lock(4, tag, LockMode::AccessShare);
        thread2_granted = true;
    });

    // give thread 2 time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(thread2_granted == false); // still blocked

    // release exclusive lock
    lm.release_lock(3, tag);

    t2.join();
    assert(thread2_granted == true); // now granted
    std::cout << "Blocking test passed\n";
}


int main() {
    access_share_lock_test();
    RowShare_lock_test();
    RowExclusiveShare_lock_test();
    test_blocking_lock();
    std::cout << "lock_manager_test: ok\n";
    return 0;
}