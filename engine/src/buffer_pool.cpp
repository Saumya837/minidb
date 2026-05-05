#include "buffer_pool.h"
namespace minidb{

    BufferPoolManager::BufferPoolManager(uint32_t pool_size, PageStore* pagestore){
        pool_size_ = pool_size;
        page = new Page[pool_size_]; //Created page array as buffer pool frames
        buffer_descriptors = new BufferDesc[pool_size_]; //Created buffer descriptor array to store metadata for each buffer frame
        clock_hand_ = 0;
        this->pagestore = pagestore;
        for(int i = 0; i < pool_size_; i++){
            buffer_descriptors[i].buf_id = i;
            buffer_descriptors[i].free_next = (i + 1 < pool_size_)  ? i + 1 : -1;
            buffer_descriptors[i].state.store(0);
        }
        free_list_head_ = 0;
    }

    BufferPoolManager::~BufferPoolManager(){
        delete[] page;
        delete[] buffer_descriptors;
    }

    Page* BufferPoolManager::fetch_page(const BufferTag& tag) {
    // cache hit
    auto it = buffer_map.find(tag);
    if (it != buffer_map.end()) {
        int buf_id = it->second;
        // TODO: increment pin count in state
        // TODO: increment usage count in state
        return &page[buf_id];
    }
    // cache miss path next
}

}