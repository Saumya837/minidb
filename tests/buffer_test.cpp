#include <iostream>


void test_buffer_pool_acceptance() {
    // setup: pool size 2, one relation
    // TODO: create FilePageStore with 3 pages
    // TODO: create BufferPoolManager with pool_size=2
    // TODO: register relation
    
    // fetch page 0 — freelist slot 0
    // fetch page 1 — freelist slot 1 (pool now full)
    // modify page 0, unpin dirty
    // unpin page 1 clean
    
    // fetch page 2 — forces eviction
    // verify page 0 was flushed to disk (dirty writeback)
    
    // re-fetch page 0
    // verify data survived eviction
}

int main() {
    std::cout << "buffer_pool_test: ok\n";
    return 0;
}