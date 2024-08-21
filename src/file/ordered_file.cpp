#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>

#include <util/record.hpp>

#include "ordered_file.hpp"


template <typename data_type>
mmap::ordered_file<data_type>::ordered_file
(
    // Required parameters
    const std::string            &file_path,
    const size_type               file_capacity,

    // Advanced parameters
    const size_type               file_index,
    const address_type            base_address,

    // System call flags
    const sys::file::flag_code    open_flag,
    const sys::file::flag_code    lock_flag,
    const sys::memory::flag_code  protocol_flag,
    const sys::memory::flag_code  mapping_flag,
    const sys::memory::flag_code  sync_flag,
    const sys::memory::flag_code  remap_flag
):  file
    (
        file_path, 
        file_capacity * sizeof(data_type), 
        file_index * sizeof(data_type),
        base_address,
        open_flag,
        lock_flag,
        protocol_flag,
        mapping_flag,
        sync_flag,
        remap_flag
    )
{
    // Validate file path
    bool valid_path = mmap::ordered_file<data_type>::valid_path(file_path);
    if (!valid_path)
    {
        util::log::record
        (
            "File does not have a valid path: "
            "parent directory and/or file path does not exist",
            util::log::type::ABORT
        );

        throw std::runtime_error("Provided file path is invalid");
    } 

    // metadata
    this->file_path     = file_path;
    this->file_capacity = file_capacity * sizeof(data_type);
    this->file_offset   = file_index * sizeof(data_type);
    this->base_address  = base_address;

    // flags
    this->open_flag     = open_flag;
    this->lock_flag     = lock_flag;
    this->protocol_flag = protocol_flag;
    this->mapping_flag  = mapping_flag;
    this->sync_flag     = sync_flag;
    this->remap_flag    = remap_flag;
}

template <typename data_type>
mmap::ordered_file<data_type>::~ordered_file() noexcept
{
    if (this->file_address)
        this->close();
}


template <typename data_type>
mmap::address_type
inline mmap::ordered_file<data_type>::open() noexcept
{
    return this->open_and_map
    (
        this->open_flag,
        this->lock_flag,
        this->protocol_flag,
        this->mapping_flag
    );
}

template <typename data_type>
mmap::address_type
mmap::ordered_file<data_type>::open_and_map
(
    sys::file::flag_code open_flag,
    sys::file::flag_code lock_flag,
    sys::file::flag_code protocol_flag,
    sys::file::flag_code mapping_flag
) noexcept
{
    // Check if mapped
    if (this->file_address)
    {
        util::log::record
        (
            "Memory is already mapped to file; "
            "call remap for reallocation",
            util::log::type::ERROR
        );

        return nullptr;
    }
    
    // Check file path is still valid
    bool valid_path = mmap::ordered_file<data_type>::valid_path(file_path);
    if (!valid_path)
    {
        util::log::record
        (
            "File does not have a valid path: "
            "parent directory and/or file path does not exist",
            util::log::type::ERROR
        );

        return nullptr;
    }

    // Open file and get file descriptor
    sys::file::descriptor file_descriptor = sys::file::open
    (
        this->file_path.c_str(), 
        open_flag
    );
    if (file_descriptor == mmap::INTERNAL_ERROR_CODE)
    {
        util::log::record
        (
            "Unable to open file and failed to recieve file descriptor",
            util::log::type::ERROR
        );

        return nullptr;
    }
    this->file_descriptor = file_descriptor;

    // Lock file
    sys::file::status_code lock_status = sys::file::lock
    (
        this->file_descriptor,
        lock_flag
    );
    if (lock_status == mmap::INTERNAL_ERROR_CODE)
    {
        util::log::record
        (
            "Unable to place lock on file for mapping process",
            util::log::type::ERROR
        );

        return nullptr;
    }

    // Resize file
    sys::file::status_code resize_status = sys::file::resize
    (
        this->file_descriptor,
        this->file_capacity
    );
    if (resize_status == mmap::INTERNAL_ERROR_CODE)
    {
        util::log::record
        (
            "Unable to resize file to provided size",
            util::log::type::ERROR
        );

        return nullptr;
    }

    // Map file
    address_type file_address = sys::memory::map
    (
        base_address,
        this->file_capacity, 
        protocol_flag,
        mapping_flag,
        this->file_descriptor, 
        this->file_offset
    );
    if (file_address == nullptr)
    {
        util::log::record
        (
            "Unable to allocate a mapping from a file address to file",
            util::log::type::ERROR
        );

        return nullptr;
    }
    this->file_address = file_address;

    return this->file_address;
}


template <typename data_type>
mmap::status_code
inline mmap::ordered_file<data_type>::flush() noexcept
{
    return this->flush
    (
        this->file_address,
        this->file_capacity,
        this->sync_flag
    );
}

template <typename data_type>
mmap::status_code
mmap::ordered_file<data_type>::flush
(
    mmap::address_type     file_address,
    mmap::size_type        size,
    sys::memory::flag_code sync_flag
) noexcept
{
    // Check if mapped
    if (this->file_address)
    {
        util::log::record
        (
            "Memory is not yet mapped",
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }
    
    // Check file path is still valid
    bool valid_path = mmap::ordered_file<data_type>::valid_path(file_path);
    if (!valid_path)
    {
        util::log::record
        (
            "File does not have a valid path: "
            "parent directory and/or file path does not exist",
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }

    // Flush kernel buffer to file
    sys::memory::status_code sync_status = sys::memory::sync
    (
        file_address,
        size * sizeof(data_type),
        sync_flag
    );
    if (sync_status == mmap::INTERNAL_ERROR_CODE)
    {
        std::size_t bound 
            = reinterpret_cast<std::size_t>(file_address) 
            + size * sizeof(data_type);

        std::stringstream stream;
        stream << "Unable to synchronize data chunk from address "
            << "0x" << std::hex << file_address << " to "<< "0x" << bound;

        util::log::record
        (
            stream.str(),
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }
    this->sync_flag = sync_flag;

    return mmap::GLOBAL_SUCCESS_CODE;
}


template <typename data_type>
mmap::status_code
inline mmap::ordered_file<data_type>::close() noexcept
{
    return this->close_and_unmap(this->sync_flag);
}

template <typename data_type>
mmap::status_code
mmap::ordered_file<data_type>::close_and_unmap
(
    sys::memory::flag_code sync_flag
) noexcept
{
    // Check if mapped
    if (this->file_address)
    {
        util::log::record
        (
            "Memory is not yet mapped",
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }

    // Check file path is still valid
    bool valid_path = mmap::ordered_file<data_type>::valid_path(file_path);
    if (!valid_path)
    {
        util::log::record
        (
            "File does not have a valid path: "
            "parent directory and/or file path does not exist",
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }

    // Flush data
    mmap::status_code flush_status = this->flush
    (
        MS_SYNC,
        this->file_address,
        this->file_capacity
    );
    if (flush_status == mmap::EXTERNAL_ERROR_CODE)
    {
        util::log::record
        (
            "Unable to flush changes to kernel buffer",
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }

    // Unmap file
    sys::memory::status_code unmap_status = sys::memory::unmap
    (
        this->file_address,
        this->file_capacity
    );
    if (unmap_status == mmap::INTERNAL_ERROR_CODE)
    {
        util::log::record
        (
            "Unable to unmap file from file address",
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }

    // Unlock file 
    sys::file::status_code unlock_status = sys::file::unlock
    (
        this->file_descriptor,
        LOCK_UN
    );
    if (unlock_status == INTERNAL_ERROR_CODE)
    {
        util::log::record
        (
            "Unable to unlock file",
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }

    // Close file
    sys::file::status_code close_status = sys::file::close
    (
        this->file_descriptor
    );
    if (close_status == INTERNAL_ERROR_CODE)
    {
        util::log::record
        (
            "Unable to close file",
            util::log::type::ERROR
        );

        return mmap::EXTERNAL_ERROR_CODE;
    }    

    return mmap::GLOBAL_SUCCESS_CODE;
}


template <typename data_type>
mmap::address_type mmap::ordered_file<data_type>::remap
(
    const size_type file_capacity
) noexcept
{
    return this->remap
    (
        this->base_address,
        file_capacity,
        this->remap_flag
    );
}

template <typename data_type>
mmap::address_type mmap::ordered_file<data_type>::remap
(
    const address_type  base_address,
    const size_type     file_capacity,
    const flag_code     remap_flag
) noexcept
{
    // Check if mapped
    if (this->file_address)
    {
        util::log::record
        (
            "Memory is not yet mapped",
            util::log::type::ERROR
        );

        return nullptr;
    }

    // Remap file
    address_type file_address = sys::memory::remap
    (
        base_address,
        this->file_capacity, 
        file_capacity * sizeof(data_type),
        remap_flag
    );
    if (file_address == nullptr)
    {
        util::log::record
        (
            "Unable to reallocate a mapping from a file address to file",
            util::log::type::ERROR
        );

        return nullptr;
    }
    this->file_address = file_address;
   
    return this->file_address;
}
