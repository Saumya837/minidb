#include "buffer.h"
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
            buffer_descriptors[buf_id].state.fetch_add(1u << BUF_REFCOUNT_SHIFT);
        
            // TODO: increment usage count in state
            uint32_t old_count = buffer_descriptors[buf_id].state.load();
            uint32_t usage = (old_count & BUF_USAGECOUNT_MASK) >> BUF_USAGECOUNT_SHIFT;
            if(usage < 5){
                buffer_descriptors[buf_id].state.fetch_add(1u << BUF_USAGECOUNT_SHIFT);
            }
            return &page[buf_id];
        }
        //cache miss
        else{
            //first check the free list head
                // if(free_list_head_ != -1){
                //     //advance freelist to next slot 
                //     int buf_id = free_list_head_;
                //     free_list_head_ = buffer_descriptors[buf_id].free_next;

                //     //take slot from the free list
                //     pagestore->read_page()
                    
                    

                //     FilePageStore store(path);

                //     store.read_page(tag.blockNum, /*how to find this page pointer i think we should use the forknum for this*/);
                // }
                // else{
                //     //run the clock sweep to find the victim
                //     //check the victim whether it is dirty 
                //     //if dirty then write it to persistent storage
                //     //else just evict the page and load the new page
                // }
           
            }
 
        }

}