#include "buffer.h"
namespace minidb{

    BufferManager::BufferManager(uint32_t pool_size){
        pool_size_ = pool_size;
        bufferPool = new Page[pool_size_]; //Created page array as buffer pool frames
        buffer_descriptors = new BufferDesc[pool_size_]; //Created buffer descriptor array to store metadata for each buffer frame
        clock_hand_ = 0;

        for(int i = 0; i < pool_size_; i++){
            buffer_descriptors[i].buf_id = i;
            buffer_descriptors[i].free_next = (i + 1 < pool_size_)  ? i + 1 : -1;
            buffer_descriptors[i].state.store(0);
        }
        free_list_head_ = 0;
    }

    BufferManager::~BufferManager(){
        delete[] bufferPool;
        delete[] buffer_descriptors;
    }

    void BufferManager::register_relation(uint32_t relnumber, PageStore* store){
        pagestore_map_[relnumber] = store;
    }

    Page* BufferManager::get_page(int buf_id) {
        return &bufferPool[buf_id];
    }

    void BufferManager::flush_page(int buf_id){
        //first 
        BufferTag& tag= buffer_descriptors[buf_id].tag;
        PageStore* store = pagestore_map_[tag.relnumber];
        store->write_page(tag.blockNum, bufferPool[buf_id]);
    }

    int BufferManager::clock_sweep(){
        uint32_t attempts = 0;
        while(true){
            while(attempts <= pool_size_ * 2){
                uint32_t state = buffer_descriptors[clock_hand_].state.load();
                uint32_t pin = (state & BUF_REFCOUNT_MASK);
                uint32_t usage = (state & BUF_USAGECOUNT_MASK);

                if(pin == 0){
                    if(usage == 0){
                        int victim = clock_hand_;
                        clock_hand_ = (clock_hand_ + 1) % pool_size_;
                        return victim;
                    }
                    buffer_descriptors[clock_hand_].state.fetch_sub(1u << BUF_USAGECOUNT_SHIFT);
                }
                clock_hand_ = (clock_hand_ + 1) % pool_size_;
                attempts++;
            }
            return -1;
        }
    }

    int BufferManager::fetch_page(const BufferTag& tag) {
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
            return buf_id;
        }
        //cache miss
        else{
                //first check the free list head
            if(free_list_head_ != -1){
                //advance freelist to next slot 
                int buf_id = free_list_head_;
                free_list_head_ = buffer_descriptors[buf_id].free_next;

                //first find the relation number
                auto store_it = pagestore_map_.find(tag.relnumber);
                if (store_it == pagestore_map_.end()) 
                    throw std::runtime_error("Relation not registered: " + std::to_string(tag.relnumber));

                //take slot from the free list
                PageStore* store = store_it->second;
                store->read_page(tag.blockNum, bufferPool[buf_id]); // read the blocknumber to the page[buf_id]

                //set the usage and ref count to 1
                buffer_descriptors[buf_id].tag = tag;
                buffer_descriptors[buf_id].state.store(1u << BUF_REFCOUNT_SHIFT | 1u << BUF_USAGECOUNT_SHIFT | BUF_VALID_FLAG);
               
                buffer_map[tag] = buf_id;
                return buf_id;
            }

            else{
                int buf_id = clock_sweep();
                if (buf_id == -1)
                    throw std::runtime_error("Buffer pool exhausted — all pages pinned.");
                uint32_t state = buffer_descriptors[buf_id].state.load();
                if(state & BUF_DIRTY_FLAG)
                    flush_page(buf_id);
                
                //set the usage and ref count to 1
                auto store_it = pagestore_map_.find(tag.relnumber);
                if (store_it == pagestore_map_.end()) 
                    throw std::runtime_error("Relation not registered: " + std::to_string(tag.relnumber));

                //take slot from the free list
                PageStore* store = store_it->second;
                store->read_page(tag.blockNum, bufferPool[buf_id]); // read the blocknumber to the page[buf_id]

                buffer_map.erase(buffer_descriptors[buf_id].tag);
                buffer_descriptors[buf_id].tag = tag;
                buffer_map[tag] = buf_id;

                //set the usage and ref count to 1
                buffer_descriptors[buf_id].state.store(1u << BUF_REFCOUNT_SHIFT | 1u << BUF_USAGECOUNT_SHIFT | BUF_VALID_FLAG);
                return buf_id;
            }
        }  
    }

    void BufferManager::mark_dirty(int buf_id){
        buffer_descriptors[buf_id].state.fetch_or(BUF_DIRTY_FLAG);
    }

    void BufferManager::unpin_page(const BufferTag& tag, bool is_dirty) {
        int buf_id = buffer_map[tag];

        buffer_descriptors[buf_id].state.fetch_sub(1u <<  BUF_REFCOUNT_SHIFT);
        if(is_dirty){
            BufferManager::mark_dirty(buf_id);
        }
    }
}