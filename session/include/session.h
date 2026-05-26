#include "../engine/include/types.h"
#include "../engine/include/transaction_manager.h"
#include "../engine/include/buffer.h"
#include "../server/include/server.h"
#include "../engine/include/lock_manager.h"

#pragma once

namespace minidb{
    struct Session {
        uint32_t session_id;
        int conn_fd;
        TransactionId current_xid;
        TransactionManager* tm;
        BufferManager* bm;
        LockManager* lm;
        std::string state;  // "idle", "active", "idle in transaction"
    };

    

}





