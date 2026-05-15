#include "lock_manager.h"

namespace minidb{
    
    bool LockManager::is_compatible(uint8_t grant_mask, LockMode mode) {

        static const uint8_t incompat[8] = {
            0b10000000,  // Share
            0b11000000,  // RowExclusive
            0b11110000,  // Exclusive 
            0b11111000,  //ShareUpdateExclusive
            0b11101100,  //SHARE
            0b11111100,  //ShareRowExclusive 
            0b11111110,  //Exclusive
            0b11111111   //AccessExclusive
        };
        
        uint8_t idx = static_cast<uint8_t>(mode);
        return (grant_mask & incompat[idx]) == 0;
    }

    
};