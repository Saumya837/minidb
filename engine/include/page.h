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
        uint16_t pd_lower; //
        uint16_t pd_upper; //
        uint16_t pd_special; //
        uint16_t pd_pagesize_version; //
        uint16_t pd_prune_xid; //
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

        Page(){
            memset(data, 0, PAGE_SIZE);
            PageHeader* hdr = header();
            hdr->pd_pagesize_version = PAGE_SIZE | 0x4;
            hdr->pd_lower = sizeof(PageHeader);
            hdr->pd_upper = PAGE_SIZE;
            hdr->pd_special = PAGE_SIZE;
            hdr->pd_prune_xid = 0;
        }
        
        PageHeader* header(){
            return reinterpret_cast<PageHeader*>(data);
        }

        const PageHeader* header() const{
            return reinterpret_cast<const PageHeader*>(data);
        }

        //Number of line pointers currently on the page
        uint16_t num_line_pointers() const {
            return ((header()->pd_lower - sizeof(PageHeader))/sizeof(LinePointer));
        }

        //Free space available on the page
        uint16_t free_space() const{
            return header()->pd_upper - header()->pd_lower;
        }

        //Insertion raw tuples bytes into page
        //Return line pointers index (1-based, matching Postgres convention)
        //Return 0 when page is full
        uint16_t insert_tuple(const uint8_t* tuple_data, uint16_t tuple_len){
            //first check if there is enough space on the page for the new tuple and line pointer 
            uint16_t space_needed = tuple_len + sizeof(LinePointer);
            uint16_t available_space = free_space();
            if (space_needed > available_space){
                return 0; // Not enough space
            }

            PageHeader * hdr = header();
            //Tuple grow downwards from the upper end of the page

            hdr->pd_upper -= tuple_len; //Move upper pointer 
            memcpy(data + hdr->pd_upper, tuple_data, tuple_len);//copy tuple data to the page

           //Line pointers grows upwards from the lower end of the page
           LinePointer lp = LinePointer::make(hdr->pd_upper, LinePointer::LP_NORMAL, tuple_len);
           memcpy(data + hdr->pd_lower, &lp, sizeof(LinePointer));
           hdr->pd_lower += sizeof(LinePointer); //Move lp_lower pointer
           return num_line_pointers(); //Return 1-based index
        }
        
        //Retrieve tuple data using line pointer index (1-based)
        const uint8_t* get_tuple(uint16_t lp_index, uint16_t& out_len) const {
            //Check if the line pointer index is valid
            if(lp_index == 0 || lp_index  > num_line_pointers()) return nullptr;

            //for getting we from an array first we have go to first element address
            const LinePointer* lp_arr= reinterpret_cast<const LinePointer*>(
                                        data + sizeof(PageHeader));

            //accessing the required line pointer 0 based index;
            const LinePointer* lp = &lp_arr[lp_index-1];

            out_len = lp->length();
            return data + lp->offset();
        }

        //Retrieve line pointer index (1-based)
       const LinePointer* get_line_pointer(uint16_t lp_index) const {
            //Check if the line pointer index is valid
            if(lp_index == 0 || lp_index  > num_line_pointers()) return nullptr;

            //for getting we from an array first we have go to first element address
            const LinePointer* lp_arr= reinterpret_cast<const LinePointer*>(
                                        data + sizeof(PageHeader));

            //accessing the required line pointer 0 based index;
            const LinePointer* lp = &lp_arr[lp_index-1];

            return lp;
        }
    };

    static_assert(sizeof(Page) == PAGE_SIZE, "Page size must be equal to PAGE_SIZE");
}