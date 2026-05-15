#pragma once
#include "types.h"
#include "snapshot.h"
#include "list"
#include "unordered_map"

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
        AccessShare = 0,
        RowShare    = 1,
        RowExclusive = 2,
        ShareUpdateExclusive = 3,
        Share  = 4,
        ShareRowExclusive = 5,
        Exclusive = 6,
        AccessExclusive = 7
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

    struct LockEntry{
        LockTag Tag;
        uint8_t grantMask;
        uint8_t waitMask;
        std::list<LockRequest> granted;
        std::list<LockRequest> requested;
    };

   struct LockTagHash {
    size_t operator()(const LockTag& tag) const {
        size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(tag.type));
        h ^= std::hash<uint32_t>{}(tag.relnumber) + 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<uint32_t>{}(tag.block_num) + 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<uint16_t>{}(tag.offset)    + 0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};


    class LockManager {
        std::mutex latch_;
        std::unordered_map<LockTag, LockEntry, LockTagHash> lock_table_;
        std::unordered_map<TransactionId, std::vector<TransactionId>> wait_for_graph_;

    public:
        // acquire lock — grant or wait
        bool acquire_lock(TransactionId xid, const LockTag& tag, LockMode mode);
        
        // release all locks held by xid
        void release_lock(TransactionId xid, const LockTag& tag);
        void release_all_locks(TransactionId xid);

    private:
        bool is_compatible(uint8_t grant_mask, LockMode mode);
        bool detect_deadlock(TransactionId xid);
        void wake_waiters(LockEntry& entry);
    };
} 