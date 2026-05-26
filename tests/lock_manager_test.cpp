#include <iostream>
#include "lock_manager.h"
#include <cassert>


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
}


int main() {
    access_share_lock_test();
    std::cout << "lock_manager_test: ok\n";
    return 0;
}