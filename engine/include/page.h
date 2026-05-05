#pragma once
#include <cstdint>
#include <cstring>
#include <cassert>

namespace minidb{

    // Defining the page size as a constant, generally 8KB pages are implemented in most database system.
    static constexpr uint32_t PAGE_SIZE = 8192; //8KB

    //Defining the maximum number of pointers that can be storedin line line pointer array.
    static constexpr uint32_t MAX_LINE_POINTERS = 256; //256 pointers

    struct PageHeader{
        uint64_t pd_lsn; // 8bytes: WAL log sequence number
        uint16_t pd_checksum; // 2bytes: checksum for page integrity
        uint16_t pd_flags; // 2bytes: page status flags
        uint16_t pd_lower; //2bytes: offset to end of line pointer array (start of free space)
        uint16_t pd_upper; // 2bytes: offset to start of free space
        uint16_t pd_special; // 2bytes: offset to start of special space
        uint16_t pd_pagesize_version; // 2bytes: page size and version info
        uint32_t pd_prune_xid; // 4bytes: oldest transaction ID that might have tuples needing pruning
    };

    static_assert(sizeof(PageHeader) == 24, "PageHeader size must be 24 bytes");
    
    struct LinePointer{
        uint32_t data;

        static LinePointer make(uint16_t offset, uint8_t flags, uint16_t len){
            LinePointer lp;
            uint32_t offset_part = offset & 0x7FFF;
            uint32_t flag_part = flags & 0x03;
            uint32_t len_part = len & 0x7FFF;

            lp.data = (offset_part | (flag_part << 15) | (len_part << 17));
            return lp;
        }

        uint16_t offset()const {return data & 0x7FFF;}           // unpack last 15 bits offset
        uint8_t flags() const { return (data >> 15) & 0x03;}     // unpack bits 15-16 flags
        uint16_t length() const { return (data >> 17) & 0x7FFF;} // unpack bits 17-31 flags
        
        static constexpr uint8_t LP_UNUSED   = 0;
        static constexpr uint8_t LP_NORMAL   = 1;
        static constexpr uint8_t LP_REDIRECT = 2;
        static constexpr uint8_t LP_DEAD     = 3;
    };

    static_assert(sizeof(LinePointer) == 4, "LinePointer size must be 4 bytes");


    class Page{
         public:
            uint8_t data[PAGE_SIZE];

        Page();
        PageHeader* header();
        const PageHeader* header() const;
        uint16_t num_line_pointers() const;
        uint16_t free_space() const;
        uint16_t insert_tuple(const uint8_t* tuple_data, uint16_t tuple_len);
        const uint8_t* get_tuple(uint16_t lp_index, uint16_t& out_len) const;
        const LinePointer* get_line_pointer(uint16_t lp_index) const;
    };

    static_assert(sizeof(Page) == PAGE_SIZE, "Page size must be equal to PAGE_SIZE");
 
}