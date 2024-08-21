#pragma once

#include "file/file.hpp"


namespace mmap
{

template<typename data_type>
class ordered_file: public file
{
private:
    // User provided file metadata
    size_type    file_capacity;
    size_type    file_offset;

public:
    using file::address;

    ordered_file
    (
        // Required parameters
        const std::string            &file_path,
        const size_type               file_capacity,

        // Advanced parameters
        const size_type               file_index    = 0,
        const address_type            base_address  = nullptr,

        // System call flags
        const sys::file::flag_code    open_flag     = O_RDWR | O_CREAT,
        const sys::file::flag_code    lock_flag     = LOCK_SH,
        const sys::memory::flag_code  protocol_flag = PROT_READ | PROT_WRITE,
        const sys::memory::flag_code  mapping_flag  = MAP_SHARED,
        const sys::memory::flag_code  sync_flag     = MS_ASYNC,
        const sys::memory::flag_code  remap_flag    = MREMAP_MAYMOVE
    );

    virtual ~ordered_file() noexcept;
    
    // No copies permitted
    ordered_file(const ordered_file &other)           = delete;
    ordered_file operator=(const ordered_file &other) = delete; 

    address_type 
    virtual open() noexcept override;
    address_type 
    virtual open_and_map
    (
        sys::file::flag_code   open_flag,
        sys::file::flag_code   lock_flag,
        sys::memory::flag_code protocol_flag,
        sys::memory::flag_code mapping_flag
    ) noexcept;     

    status_code
    virtual flush() noexcept override;
    status_code 
    virtual flush
    (
        mmap::address_type     address,
        mmap::size_type        capacity,
        sys::memory::flag_code sync_flag
    ) noexcept;

    status_code 
    virtual close() noexcept override;
    status_code 
    virtual close_and_unmap
    (
        sys::memory::flag_code sync_flag
    ) noexcept;     

    address_type
    virtual remap
    ( 
        const size_type file_capacity
    ) noexcept override;
    address_type
    virtual remap
    (
        const address_type base_address,
        const size_type    file_capacity,
        const flag_code    remap_flag
    ) noexcept;

protected:
    using file::valid_path;
};

} // mmap namespace
