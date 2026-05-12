#pragma once
#include "types.h"
#include "snapshot.h"

namespace minidb {

    enum class LockWaitOper {
        None,
        Update,      // waiting to update a tuple
        Delete,      // waiting to delete a tuple
        Lock,        // waiting for explicit lock (SELECT FOR UPDATE)
    };

    // TODO: LockTag — identifies what is being locked (relation, tuple, page)
    enum class LockTagType : uint8_t {
        Relation = 0,  // locking entire table
        Page     = 1,  // locking a page
        Tuple    = 2,  // locking a specific tuple
    };

    struct LockTag {
        LockTagType type;
        uint32_t    relnumber;   // which relation
        uint32_t    block_num;   // which page (0 for relation locks)
        uint16_t    offset;      // which tuple (0 for page/relation locks)

        bool operator==(const LockTag& other) const {
            return type == other.type &&
                relnumber == other.relnumber &&
                block_num == other.block_num &&
                offset == other.offset;
        }
    };

    enum class LockMode : uint8_t {
        None      = 0,
        Share     = 1,  
        Update    = 2,  
        Exclusive = 3, 
    };

    struct LockRequest 
    {
        TransactionId xid;
        LockMode mode;
        LockWaitOper wait_oper;
        bool granted;
        std::unique_ptr<std::condition_variable> cv;
        std::unique_ptr<std::mutex> cv_mutex;

        LockRequest(TransactionId xid, LockMode mode, LockWaitOper oper)
            : xid(xid), mode(mode), wait_oper(oper), granted(false),
            cv(std::make_unique<std::condition_variable>()),
            cv_mutex(std::make_unique<std::mutex>()) {}
    };



} 