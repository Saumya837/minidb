#include "page.h"

namespace minidb{
        PageHeader* Page::header() {
            return reinterpret_cast<PageHeader*>(data);
        }

        const PageHeader* Page::header() const{
            return reinterpret_cast<const PageHeader*>(data);
        }

        uint16_t Page::num_line_pointers() const{
            return ((header()->pd_lower - sizeof(PageHeader))/ sizeof(LinePointer));
        }

        uint16_t Page::free_space() const{
            return (header()->pd_upper - header()->pd_lower);
        }

        uint16_t Page::insert_tuple(const uint8_t* tuple_data, uint16_t tuple_len){
            uint16_t space_needed = tuple_len + sizeof(LinePointer);
            uint16_t available_space = free_space();
            if(space_needed > available_space) return 0; //Space not enough

            PageHeader *hdr = header();

            hdr->pd_upper -= tuple_len; //Move upper pointer
            memcpy(data + hdr->pd_upper, tuple_data, tuple_len);

            LinePointer lp = LinePointer::make(hdr->pd_upper, LinePointer::LP_NORMAL, tuple_len);
            memcpy(data + hdr->pd_lower, &lp, sizeof(LinePointer));
            hdr->pd_lower += sizeof(LinePointer); //Move Lower Pointer

            return num_line_pointers(); //Return 1-based index
        }
        
        //Retrieve tuple data using line pointer index (1-based)
        const uint8_t* Page::get_tuple(uint16_t lp_index, uint16_t& out_len) const {
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
       const LinePointer* Page::get_line_pointer(uint16_t lp_index) const {
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