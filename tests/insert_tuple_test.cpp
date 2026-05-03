#include "transaction_manager.h"
#include "tuple.h"
#include "types.h"
#include "page.h"
#include "pagestore.h"
#include <cassert>
#include <iostream>

using namespace minidb;

void insert_test(){
    TransactionManager tm;
    TransactionId first_xid = tm.begin();

    TupleHeader hdr;
    hdr.xmin = first_xid;
    hdr.xmax = 0;
    hdr.cmin = 0;
    hdr.infomask = HEAP_XMIN_IS_SET;
    hdr.natts = 1;

    uint32_t value = 42;  // one integer column
    uint16_t data_size = sizeof(uint32_t);

    uint8_t buf[16 + data_size];
    memcpy(buf, &hdr, sizeof(TupleHeader));           // copy header
    memcpy(buf + sizeof(TupleHeader), &value, data_size); // copy data after

    remove("/tmp/minidb_test.db");
    FilePageStore store("/tmp/minidb_test.db");
    uint32_t page0 = store.allocate_page();

    Page page;
    uint32_t lp_index = page.insert_tuple(buf, sizeof(buf));
    assert(lp_index == 1);

    store.write_page(page0, page);
    TransactionId second_xid = tm.begin();
    Snapshot snap_b = tm.get_snapshot(second_xid, 0); 

    tm.commit_transaction(first_xid);
    TransactionId third_xid = tm.begin();

    store.read_page(page0, page);

    uint16_t out_len;
    const uint8_t* out_data = page.get_tuple(lp_index, out_len);
    assert(out_len == sizeof(buf));

    TupleHeader* recovered_hdr = reinterpret_cast<TupleHeader*>(const_cast<uint8_t*>(out_data));
    if(recovered_hdr->infomask & HEAP_XMIN_IS_SET && tm.get_transaction_status(recovered_hdr->xmin) == XactStatus::Commited)
        recovered_hdr->infomask = recovered_hdr->infomask & (~HEAP_XMIN_IS_SET) | HEAP_XMIN_COMMITTED;
    
    Snapshot snap_c = tm.get_snapshot(third_xid, 0); // just to update transaction status
    assert(tuple_is_visible(*recovered_hdr, snap_b) == false);
    assert(tuple_is_visible(*recovered_hdr, snap_c) == true);

    uint32_t* recovered_value = reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(out_data + sizeof(TupleHeader)));
    assert(*recovered_value == 42); 
}

int main(){
    insert_test();
    std::cout<< "Insert test passed!" << std::endl;
    return 0;
};
