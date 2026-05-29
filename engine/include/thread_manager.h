/*
    * ThreadManager — manages backend threads in the minidb engine.
    *
    * When a transaction is blocked by the lock manager, it is placed in a
    * wait queue and automatically woken when the lock it is waiting on is
    * released.
    *
    * In PostgreSQL this is handled by PGPROC (src/include/storage/proc.h).
    * minidb lock_v0 uses a simplified thread-per-transaction model without
    * the full PGPROC infrastructure. PGPROC support is planned for lock_v1.
*/

#pragma once 
#include "types.h"
#include <thread>

    namespace minidb{
        /*
            The Class ThreadState is used to represent the state of a transaction,
            during it excution by our engine.
        */

        enum class ThreadState{
            Running,
            Blocked,
            Done
        };

        /*
            * Thread — represents a single backend transaction thread.
            *
            * Each transaction that enters the engine is assigned a Thread entry.
            * The ThreadManager uses this to track execution state and coordinate
            * blocking/wakeup when lock contention occurs.
            *
            * Fields:
            *   xid    — transaction ID, ties this thread to its TransactionManager entry
            *   state  — current execution state (Running, Blocked, Done)
            *   handle — OS thread handle for join/detach
            *   cv     — condition variable used to sleep/wake on lock state changes
            *   mtx    — mutex paired with cv (required by std::condition_variable)
            *
            * Note: cv and mtx are heap-allocated via unique_ptr because
            * std::condition_variable is neither copyable nor movable.
            *
            * PostgreSQL equivalent: PGPROC (src/include/storage/proc.h)
            * Planned for lock_v1: full PGPROC-style per-backend state tracking.
        */

        struct Thread{
            TransactionId xid;
            ThreadState state;
            std::thread handle;
            std::unique_ptr<std::condition_variable>  cv;
            std::unique_ptr<std::mutex> mtx;

            Thread(TransactionId xid, std::thread handle):
                xid(xid), state(ThreadState::Running),
                handle(std::move(handle)), cv(std::make_unique<std::condition_variable>()),
                mtx(std::make_unique<std::mutex>()){}
        };

        class ThreadManager{
        };
}
