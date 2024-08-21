#include <exception>
#include <filesystem>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>

#include <util/record.hpp>

#include "file.hpp"


mmap::file::file
(
    // Required parameters
    const std::string            &file_path,
    const size_type               file_capacity_bytes,

    // Advanced parameters
    const size_type               file_offset_bytes,
    const address_type            base_address,

    // System call flags
    const sys::file::flag_code    open_flag,
    const sys::file::flag_code    lock_flag,
    const sys::memory::flag_code  protocol_flag,
    const sys::memory::flag_code  mapping_flag,
    const sys::memory::flag_code  sync_flag,
    const sys::memory::flag_code  remap_flag
)
{
    // Validate file path
    bool valid_path = mmap::file::valid_path(file_path);
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
    this->file_path           = file_path;
    this->file_capacity_bytes = file_capacity_bytes;
    this->file_offset_bytes   = file_offset_bytes;
    this->base_address        = base_address;

    // flags
    this->open_flag     = open_flag;
    this->lock_flag     = lock_flag;
    this->protocol_flag = protocol_flag;
    this->mapping_flag  = mapping_flag;
    this->sync_flag     = sync_flag;
    this->remap_flag    = remap_flag;
}

mmap::file::~file() noexcept
{
    if (this->file_address)
        this->close();
}


mmap::address_type
inline mmap::file::open() noexcept
{
    return this->open_and_map
    (
        this->open_flag,
        this->lock_flag,
        this->protocol_flag,
        this->mapping_flag
    );
}

mmap::address_type
mmap::file::open_and_map
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
    bool valid_path = mmap::file::valid_path(file_path);
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
        this->file_capacity_bytes
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
        this->file_capacity_bytes, 
        protocol_flag,
        mapping_flag,
        this->file_descriptor, 
        this->file_offset_bytes
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


mmap::status_code
inline mmap::file::flush() noexcept
{
    return this->flush
    (
        this->file_address,
        this->file_capacity_bytes,
        this->sync_flag
    );
}

mmap::status_code
mmap::file::flush
(
    mmap::address_type     file_address,
    mmap::size_type        chunk_size_bytes,
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
    bool valid_path = mmap::file::valid_path(file_path);
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
        chunk_size_bytes,
        sync_flag
    );
    if (sync_status == mmap::INTERNAL_ERROR_CODE)
    {
        std::size_t bound 
            = reinterpret_cast<std::size_t>(file_address) 
            + chunk_size_bytes;

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


mmap::status_code
inline mmap::file::close() noexcept
{
    return this->close_and_unmap(this->sync_flag);
}

mmap::status_code
mmap::file::close_and_unmap
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
    bool valid_path = mmap::file::valid_path(file_path);
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
        this->file_address,
        this->file_capacity_bytes,
        MS_SYNC
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
        this->file_capacity_bytes
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


mmap::address_type mmap::file::remap
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

mmap::address_type mmap::file::remap
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
        this->file_capacity_bytes, 
        file_capacity,
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


mmap::address_type 
mmap::file::address() const noexcept
{
    return this->file_address;
}


bool 
inline mmap::file::valid_path
(
    const std::string &file_path
) noexcept
{
    try 
    {
        std::filesystem::path path(file_path);
        if (std::filesystem::exists(path))
            return true;
        
        std::filesystem::path parent_dir = path.parent_path();
        if (!parent_dir.empty() && std::filesystem::exists(parent_dir))
            return true;

        return false;
    }

    catch (const std::exception &exception)
    {
        return false;
    }
}
