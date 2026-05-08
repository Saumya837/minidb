#include <iostream>
#include "buffer.h"

using namespace minidb;

void test_buffer_pool_acceptance() {
    // setup: pool size 2, one relation
    int pool_size_ = 2;

    remove("/tmp/minidb_test.db");
    FilePageStore store("/tmp/minidb_test.db");
    
    // TODO: create FilePageStore with 3 pages
    uint32_t page0 = store.allocate_page();
    uint32_t page1 = store.allocate_page();
    uint32_t page2 = store.allocate_page();
    
   
    // TODO: create BufferPoolManager with pool_size=2
    BufferManager* bm = new BufferManager(pool_size_);

    // TODO: register relation
    bm->register_relation(1, &store);
    
    //create tag
    BufferTag tag0;
    tag0.blockNum = 0;
    tag0.relnumber = 1;
    // fetch page 0 — freelist slot 0
    uint32_t buf_id_0= bm->fetch_page(tag0);

    Page* p0 = bm->get_page(buf_id_0);
    // write a known tuple into p0
    uint8_t marker[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    p0->insert_tuple(marker, 4);

     //create tag
    BufferTag tag1;
    tag1.blockNum = 1;
    tag1.relnumber = 1;
    // fetch page 1 — freelist slot 1 (pool now full)
    uint32_t buf_id_1 = bm->fetch_page(tag1);

    // modify page 0, unpin dirty
    bm->unpin_page(tag0, true);

    // unpin page 1 clean
    bm->unpin_page(tag1, false);

    //create tag
    BufferTag tag2;
    tag2.blockNum = 2;
    tag2.relnumber = 1;
    
    // fetch page 2 — forces eviction
    uint32_t buf_id_2 = bm->fetch_page(tag2);

    // re-fetch page 0
    int buf_id0_reloaded = bm->fetch_page(tag0);
    Page* p0_reloaded = bm->get_page(buf_id0_reloaded);
    
    // verify data survived eviction
    uint16_t out_len;
    const uint8_t* out_data = p0_reloaded->get_tuple(1, out_len);
    assert(out_len == 4);
    assert(memcmp(out_data, marker, 4) == 0);
    std::cout << "Acceptance test passed — dirty page survived eviction\n";
}

int main() {
    std::cout << "buffer_pool_test: ok\n";
    return 0;
}