#include "lock_manager.h"
#include <stack>

namespace minidb{

    
    bool LockManager::is_compatible(uint8_t grant_mask, LockMode mode) {

        static const uint8_t incompat[8] = {
            0b10000000,  //AccessShare
            0b11000000,  //RowExclusive
            0b11110000,  //Exclusive 
            0b11111000,  //ShareUpdateExclusive
            0b11101100,  //SHARE
            0b11111100,  //ShareRowExclusive 
            0b11111110,  //Exclusive
            0b11111111   //AccessExclusive
        };
        
        uint8_t idx = static_cast<uint8_t>(mode);
        return (grant_mask & incompat[idx]) == 0;
    }

    bool LockManager::detect_cycle(TransactionId xid, std::unordered_set<TransactionId>& visited, 
        std::unordered_set<TransactionId>& in_stack, std::unordered_map<TransactionId, std::vector<TransactionId>> graph){

        visited.insert(xid);
        in_stack.insert(xid);
        auto it = graph.find(xid);

        if (it != wait_for_graph_.end()) {
            for (TransactionId neighbor : it->second) {
                if (visited.count(neighbor) == 0) {
                    if (detect_cycle(neighbor, visited, in_stack, graph))
                        return true;
                } else if (in_stack.count(neighbor) > 0) 
                    return true; // cycle found
            }
        }

        in_stack.erase(xid);
        return false;
    }

    bool LockManager::detect_deadlock(TransactionId xid){
        std::unordered_map<TransactionId, std::vector<TransactionId>> graph;
        for (auto& [tag, entry] : lock_table_) {
            for (auto& waiter : entry.requested) {
                for (auto& holder : entry.granted) {
                    graph[waiter.xid].push_back(holder.xid);
                }
            }
        }

        std::unordered_set<TransactionId> visited;
        std::unordered_set<TransactionId> in_stack;
        return detect_cycle(xid, visited, in_stack, graph);
    }

    bool LockManager::acquire_lock(TransactionId xid, const LockTag& tag, LockMode mode){

            // 1. lock latch_
            std::unique_lock<std::mutex> lock(latch_);

            // 2. find or create LockEntry for tag in lock_table_
            auto& entry = lock_table_[tag];
            entry.Tag = tag;

            // 3. check is_compatible(entry.grant_mask, mode)
            if(is_compatible(entry.grantMask, mode)){

                // - add xid to granted list
                entry.granted.emplace_back(xid, mode, LockWaitOper::None);

                // - update grant_mask
                entry.grantMask |= (1u << static_cast<uint8_t>(mode));

                // - return true
                return true;
            }
            else{
                    // - add xid to waiting list
                    entry.requested.emplace_back(xid, mode, LockWaitOper::Lock);

                    // - update wait_mask
                    entry.waitMask |= (1u << static_cast<uint8_t>(mode));

                    // - add edge xid -> in wait_for_graph_
                    for(auto& req: entry.granted){
                        wait_for_graph_[xid].push_back(req.xid);
                }

                if(detect_deadlock(xid)){
                    entry.requested.remove_if([xid](const LockRequest& r){ return r.xid == xid; });
                    entry.waitMask &= ~(1u << static_cast<uint8_t>(mode));
                    // clean up wait-for graph
                    wait_for_graph_.erase(xid);
                    release_all_locks(xid);
                    throw std::runtime_error("Deadlock detected — transaction " + std::to_string(xid) + " aborted");
                }

                // after deadlock check, before "woken up" comment
                auto& req = entry.requested.back();
                auto* cv_ptr = req.cv.get(); // capture pointer before potential move
                cv_ptr->wait(lock, [&entry, xid]{
                    for (auto& r : entry.granted)
                        if (r.xid == xid) return true;
                    return false;
                });

                // woken up — lock granted
                entry.grantMask |= (1u << static_cast<uint8_t>(mode));
                entry.waitMask &= ~(1u << static_cast<uint8_t>(mode));
                wait_for_graph_.erase(xid);
                return true;
            } 
        }

        void LockManager::release_lock_internals(TransactionId xid, LockEntry& entry){

            entry.granted.remove_if([xid](const LockRequest& r) { return r.xid == xid; });

            // 4. recalculate grant_mask from remaining granted list
            entry.grantMask = 0;
            for(auto& req: entry.granted){
                entry.grantMask |= (1u << static_cast<uint8_t>(req.mode));
            }

            // 5. wake up waiters — call wake_waiters(entry)
            wake_waiters(entry);
        }

        void LockManager::release_lock(TransactionId xid, const LockTag& tag){
            // 1. lock latch_
            std::unique_lock<std::mutex> lock(latch_);
            // 2. find LockEntry for tag
            auto& entry = lock_table_[tag];
            release_lock_internals(xid , entry);
        }

        void LockManager::wake_waiters(LockEntry& entry) {
            for (auto it = entry.requested.begin(); it != entry.requested.end(); ) {
                if (is_compatible(entry.grantMask, it->mode)) {
                    // 1. wake it up
                    it->cv->notify_one();
                    // 2. update grant_mask
                    entry.grantMask |= (1u << static_cast<uint8_t>(it->mode));
                    // 3. move from waiting to granted
                    entry.granted.emplace_back(std::move(*it));
                    it = entry.requested.erase(it);
                } 
                else 
                    ++it;
            }
        }

        void LockManager::release_all_locks(TransactionId xid){
            std::unique_lock<std::mutex> lock(latch_);

            for(auto& [tag, entry]: lock_table_){
                //call the release_lock
                release_lock_internals(xid, entry);
            }
            wait_for_graph_.erase(xid);
        }
       
};