#pragma once
#include "page.h"
#include <string>
#include <unordered_map>

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

    class FilePageStore : public PageStore {
        int fd_;
        std::string path_;
        uint32_t num_pages_;

    public:
        FilePageStore(const std::string& path);
        ~FilePageStore();

        void read_page(uint32_t page_no, Page& page) override;
        void write_page(uint32_t page_no, const Page& page) override;
        uint32_t allocate_page() override;
        uint32_t num_pages() override;
        void flush() override;
    };
}
