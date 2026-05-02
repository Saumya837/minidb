// #include "transaction_manager.h"
// #include "tuple.h"
// #include "types.h"

// using namespace minidb;

// void insert_test(){

//     TransactionManager tm;
//     TransactionId first_xid = tm.begin();

//     TupleHeader hdr;
//     hdr.xmin = first_xid;
//     hdr.xmax = 0;
//     hdr.cmin = 0;
//     hdr.infomask = HEAP_XMIN_IS_SET;
//     hdr.natts = 1;



//     uint8_t buf[16 + data_size];
//     memcpy(buf, &hdr, sizeof(TupleHeader));           // copy header
//     memcpy(buf + sizeof(TupleHeader), data, data_size); // copy data after
// }

int main(){
    // insert_test();
    return 0;
};
