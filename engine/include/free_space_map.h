#pragma once
#include "types.h"

namespace minidb {
    // TODO: tracks free space per page for insert routing
    // Mirrors Postgres FSM — avoids full page scans on INSERT
    class FreeSpaceMap {
        private:
            uint16_t _num_leaves;
        public:
            explicit FreeSpaceMap(int num_leaves): _num_leaves(num_leaves);
            uint16_t num_leaves();
        
    };
} 