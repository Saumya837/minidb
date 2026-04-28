#include<cstdint>
#include<atomic>

using OID = uint32_t;

struct BufferTag{
    OID specOID;
    OID dbOID;
    uint32_t relnumber;
    uint32_t blockNum;
    uint32_t forkNum;
};

struct BufferDesc{
        BufferTag tag;
        int buf_id;
        std::atomic<uint32_t> state;
        int free_next;
};