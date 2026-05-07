#pragma once
#include<cstdint>
#include<atomic>
#include "types.h"
#include <unordered_map>
#include "page.h"
#include "pagestore.h"




namespace minidb {

    static constexpr uint32_t BUF_REFCOUNT_SHIFT   = 22;
    static constexpr uint32_t BUF_USAGECOUNT_SHIFT = 19;
    static constexpr uint32_t BUF_DIRTY_FLAG       = (1u << 18);
    static constexpr uint32_t BUF_VALID_FLAG       = (1u << 17);
    static constexpr uint32_t BUF_IO_IN_PROGRESS   = (1u << 16);
    static constexpr uint32_t BUF_REFCOUNT_MASK    = 0x3FFu << 22;
    static constexpr uint32_t BUF_USAGECOUNT_MASK  = 0x7u  << 19;
    
    struct BufferTag{
        OID spcOID; // tablespace Id
        OID dbOID; // database Id
        uint32_t relnumber; // relational number
        uint32_t blockNum; // block number
        uint32_t forkNum; //fork Number

        bool operator==(const BufferTag& other) const {
            return 
            spcOID == other.spcOID && dbOID == other.dbOID && 
            relnumber == other.relnumber && blockNum == other.blockNum && 
            forkNum == other.forkNum;
        }
    };

    //state in buffer desc
    //bits 31-22 : refcount/pin_count  (10 bits)
    // bits 21-19 : usage_count         (3 bits)
    // bit  18    : dirty flag
    // bit  17    : valid flag
    // bit  16    : io_in_progress flag
    // bits 15-0  : reserved

    struct BufferDesc{
            BufferTag tag;
            int buf_id;
            std::atomic<uint32_t> state; 
            int free_next;
    };

    struct BufferTagHash{
        size_t operator()(const BufferTag& tag) const {
            size_t h = std::hash<uint32_t>{}(tag.blockNum);
            h ^= std::hash<uint32_t>{}(tag.relnumber) + 0x9e3779b9 + (h<<6) + (h>>2);
            h ^= std::hash<uint32_t>{}(tag.forkNum)   + 0x9e3779b9 + (h<<6) + (h>>2);
            return h;
        }
    };

    class BufferPoolManager {
            uint32_t pool_size_;// number of pages in buffer pool 
            Page* page; 
            BufferDesc* buffer_descriptors;
            uint32_t clock_hand_;
            int free_list_head_;
            std::unordered_map<uint32_t, PageStore*> pagestore_map_;
            std::unordered_map<BufferTag, int, BufferTagHash> buffer_map; // maps BufferTag to buffer index
            void flush_page(int buf_id);  // write dirty page to disk, called by eviction
            int clock_sweep();

        public:
            BufferPoolManager(uint32_t pool_size);
            ~BufferPoolManager();
            void register_relation(uint32_t relnumber, PageStore* store);
            int fetch_page(const BufferTag& tag);
            void unpin_page(const BufferTag& tag, bool is_dirty);
            void mark_dirty(int buf_id);
    };


}

