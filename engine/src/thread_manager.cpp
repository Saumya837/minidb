#include "thread_manager.h"

namespace minidb{
    void ThreadManager::run(TransactionId xid, std::function<void()> func){
        
        auto thread_entry = std::make_unique<Thread> (xid);

        //create a new thread to run the provided function
        std::thread t([this, xid, func](){
            func(); //execute the transaction's work
            std::lock_guard<std::mutex> lock(latch_);
            auto& thread = threads_[xid];
            thread->state = ThreadState::Done;
            thread->cv->notify_all();
        });

        thread_entry->handle = std::move(t);

        std::lock_guard<std::mutex> lock(latch_);
        threads_[xid] = std::move(thread_entry);
    }

    void ThreadManager::wait_until_blocked(TransactionId xid){
        Thread* t;
        {
            std::lock_guard<std::mutex> lock(latch_);  // holds latch_
            t = threads_[xid].get();
        }  // lock_guard destructs here — latch_ released
        t->blocked_future.wait();  // waits WITHOUT holding latch_
    }
}