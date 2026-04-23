#pragma once
#include "page.h"

namespace minidb{
    class PageStore{
        public:
            virtual void read_page(uint32_t page_no, Page& page) = 0;
            virtual void write_page(uint32_t page_no, const Page& page) = 0;
            virtual uint32_t allocate_page() = 0;
            virtual uint32_t num_pages() = 0;
            virtual void flush() = 0; 
            virtual ~PageStore() = default;
    };
}
