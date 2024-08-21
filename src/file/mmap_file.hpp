#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <lib/file.hpp>
#include <lib/mmap.hpp>

namespace mmap
{

using address_type = void *;
using size_type    = std::size_t;
using status_code  = std::uint8_t;
using flag_code    = std::uint8_t;

constexpr sys::file::sint_t GLOBAL_SUCCESS_CODE = EXIT_SUCCESS;
constexpr sys::file::sint_t EXTERNAL_ERROR_CODE = EXIT_FAILURE;
constexpr sys::file::sint_t INTERNAL_ERROR_CODE = -1;

template<typename data_type>
class file
{
private:
    // User provided file metadata
    std::string  file_path;
    size_type    file_size;
    size_type    file_offset;
    address_type base_address;

    // Internal file metadata
    address_type          file_address;
    sys::file::descriptor file_descriptor;

    // File operation flags
    sys::file::flag_code   open_flag       = O_RDWR | O_CREAT;
    sys::file::flag_code   lock_flag       = LOCK_SH;

    // Mapping flags
    sys::memory::flag_code protocol_flag = PROT_READ | PROT_WRITE;
    sys::memory::flag_code mapping_flag  = MAP_SHARED;

    // Synchronization flags
    sys::memory::flag_code sync_flag = MS_ASYNC;

    // Realloction flags
    sys::memory::flag_code remap_flag = MREMAP_MAYMOVE;

public:
    file
    (
        // Required parameters
        const std::string            &file_path,
        const size_type               file_size,

        // Advanced parameters
        const size_type               file_offset   = 0,
        const address_type            base_address  = nullptr,

        // System call flags
        const sys::file::flag_code    open_flag     = O_RDWR | O_CREAT,
        const sys::file::flag_code    lock_flag     = LOCK_SH,
        const sys::memory::flag_code  protocol_flag = PROT_READ | PROT_WRITE,
        const sys::memory::flag_code  mapping_flag  = MAP_SHARED,
        const sys::memory::flag_code  sync_flag     = MS_ASYNC,
        const sys::memory::flag_code  remap_flag    = MREMAP_MAYMOVE
    );

    ~file() noexcept;
    
    // No copies permitted
    file(const file &other) = delete;
    file operator=(const file &other) = delete;

    address_type 
    address() const noexcept;

    address_type 
    virtual open() noexcept;
    address_type 
    open_and_map
    (
        sys::file::flag_code   open_flag,
        sys::file::flag_code   lock_flag,
        sys::memory::flag_code protocol_flag,
        sys::memory::flag_code mapping_flag
    ) noexcept;     

    status_code
    virtual flush() noexcept;
    status_code 
    flush
    (
        mmap::address_type     address,
        mmap::size_type        capacity,
        sys::memory::flag_code sync_flag
    ) noexcept;

    status_code 
    virtual close() noexcept;
    status_code 
    close_and_unmap
    (
        sys::memory::flag_code sync_flag
    ) noexcept;     

    address_type
    virtual remap
    ( 
        const size_type file_capacity
    ) noexcept;
    address_type
    remap
    (
        const address_type base_address,
        const size_type    file_capacity,
        const flag_code    remap_flag
    ) noexcept;
    
protected:
    inline static bool valid_path
    (
        const std::string &file_path
    ) noexcept;

};

} // mmap namespace
