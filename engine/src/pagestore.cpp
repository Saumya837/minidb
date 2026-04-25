#include "pagestore.h"
#include <fcntl.h>
#include <unistd.h>
#include <string>

namespace minidb {

    FilePageStore::FilePageStore(const std::string& path) {
        // TODO: open file with open(), O_RDWR | O_CREAT, permissions 0644
        fd_ = open(path.c_str(), O_RDWR | O_CREAT, 0644);
        if(fd_ < 0)
            throw std::runtime_error("Failed to open page store file:" + path);
        path_ = path;
        // TODO: set num_pages_ based on file size
        num_pages_ = lseek(fd_, 0, SEEK_END)/PAGE_SIZE;
    }

    FilePageStore::~FilePageStore() {
        // TODO: close fd_
        if(fd_ >= 0){
            close(fd_);
        }
    }

    void FilePageStore::read_page(uint32_t page_no, Page& page){
        // TODO: bounds check — page_no must be < num_pages_
        if(page_no >= num_pages_)
            throw std::out_of_range("Page number out of range: " + std::to_string(page_no));
        off_t offset = page_no * PAGE_SIZE;

        // TODO: pread(fd_, page.data, PAGE_SIZE, offset)
        ssize_t bytes_read = pread(fd_, page.data, PAGE_SIZE, offset);

        // TODO: check return value — pread returns bytes read
        if(bytes_read != PAGE_SIZE)
            throw std::runtime_error("Failed to read full page from file:" 
                    + path_ + " at page number: " + std::to_string(page_no));

        printf("bytes_read = %zd\n", bytes_read);

    }

    void FilePageStore::write_page(uint32_t page_no, const Page& page){
        // TODO: bounds check
        if(page_no >= num_pages_)
            throw std::out_of_range("Page number out of range:" + std::to_string(page_no));
        off_t offset = page_no * PAGE_SIZE;
        // TODO: pwrite(fd_, page.data, PAGE_SIZE, offset)
        ssize_t bytes_written = pwrite(fd_, page.data, PAGE_SIZE, offset);
        // TODO: check return value
        if(bytes_written != PAGE_SIZE)
            throw std::runtime_error("Failed to write full page to file:" 
                    + path_ + " at page number: " + std::to_string(page_no));
    }

    uint32_t FilePageStore::allocate_page(){
        //TODO: create a new empty page
        Page next = Page();

        //TODO: increment num_pages_ and return the allocated page number
        num_pages_++;

        //TODO: write it to the end of the file
        write_page(num_pages_-1, next);

        return num_pages_-1; //return the allocated page number
    }

    uint32_t FilePageStore::num_pages() {
        // TODO: return num_pages_
        return num_pages_;
    }

    void FilePageStore::flush(){
        // TODO: fsync(fd_)
        if(fd_ >= 0){
            if(fsync(fd_) != 0)
                throw std::runtime_error("Failed to flush page store file:" + path_);
        }
    }
};
 // namespace minidb