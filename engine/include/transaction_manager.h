#pragma once
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include "snapshot.h"
#include "types.h"

namespace minidb{

    enum class XactStatus{
        Active,
        Commited,
        Aborted
    };

    class TransactionManager{
        private :
            TransactionId next_xid_;                                // XID counter
            std::unordered_set<TransactionId> active_xids_;              // Set od active transactions
            std::unordered_map<TransactionId, XactStatus> xact_status_;  // Transaction status map
        public :
            TransactionId begin();
            void commit_transaction(TransactionId xid);
            void abort_transaction(TransactionId xid);
            Snapshot get_snapshot(TransactionId current_xid);
            XactStatus get_transaction_status(TransactionId xid);
    };
};