#include <iostream>
#include "lock_manager.h"
#include <cassert>
#include <thread>


using namespace minidb;

void access_share_lock_test(){
    LockManager lm;
    LockTag tag;
    tag.type = LockTagType::Tuple;
    tag.relnumber = 1;
    tag.block_num = 2;
    tag.offset = 3;

    // acquire Share lock as xid=3
    bool status_3 = lm.acquire_lock(3, tag, LockMode::AccessShare);

    // TODO: acquire Share lock as xid=4
    bool status_4 = lm.acquire_lock(4, tag, LockMode::AccessShare);


    // TODO: assert both granted — no blocking
    assert(status_3 == true);
    assert(status_4 == true);
    std::cout << "Share lock test passed\n";
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
    test_blocking_lock();
    std::cout << "lock_manager_test: ok\n";
    return 0;
}