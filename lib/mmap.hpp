#pragma once


#include <concepts>
#include <cstdint>
#include <cassert>
#include <cstddef>
#include <limits>
#include <stdexcept>

#include <unistd.h>

#include "lib/type.hpp"


namespace sys
{

#include <sys/mman.h>
#include <sys/resource.h>

namespace memory
{

using status_code = sint_t;
using flag_code   = sint_t;

using address_type = void *;
using size_type    = std::size_t;

// common constants 
constexpr address_type DEFAULT_BASE = nullptr;
constexpr size_type    ZERO_SPACE   = size_type{0};
constexpr flag_code    NO_FLAG      = flag_code{0};
constexpr flag_code    NO_PROTOCOL  = flag_code{0};
constexpr flag_code    NO_ADVICE    = flag_code{0};

// mapping
inline auto &map   = mmap;
inline auto &unmap = munmap;
inline auto &remap = mremap;

// synchronicity & protection
inline auto &sync       = msync;
inline auto &lock       = mlock;
inline auto &lock_all   = mlockall;
inline auto &unlock     = munlock;
inline auto &unlock_all = munlockall;
inline auto &protect    = mprotect;

// utility
inline auto &advise = madvise;
inline auto &reside = mincore;
inline auto &repage = remap_file_pages;

// function concept groups
template <typename function_type>
concept core = std::same_as<function_type, decltype(sys::memory::map)>
            || std::same_as<function_type, decltype(sys::memory::remap)>
            || std::same_as<function_type, decltype(sys::memory::unmap)>;

template <typename function_type>
concept auxiliary = std::same_as<function_type, decltype(sys::memory::sync)>
                 || std::same_as<function_type, decltype(sys::memory::lock)>
                 || std::same_as<function_type, decltype(sys::memory::unlock)>
                 || std::same_as<function_type, decltype(sys::memory::protect)>
                 || std::same_as<function_type, decltype(sys::memory::reside)>
                 || std::same_as<function_type, decltype(sys::memory::advise)>
                 || std::same_as<function_type, decltype(sys::memory::repage)>;

template <typename function_type>
concept aggregate = std::same_as<function_type, decltype(sys::memory::lock_all)>
                 || std::same_as<function_type, decltype(sys::memory::unlock_all)>;

namespace file 
{

inline auto &link   = shm_open;
inline auto &unlink = shm_unlink;

} // shared namespace

// paging
memory::size_type
inline page_size() noexcept
{
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
}


memory::address_type 
inline page_align
(
    const memory::address_type address, 
    const memory::size_type    page_size
) noexcept
{
    using ::std::uintptr_t;
    const memory::size_type offset_mask = page_size - 1;

    assert(page_size != memory::ZERO_SPACE);
    assert((page_size & offset_mask) == memory::ZERO_SPACE);

    return reinterpret_cast<memory::address_type>
    (
        (reinterpret_cast<std::uintptr_t>(address) + offset_mask) & ~offset_mask
    );
}


memory::address_type 
inline page_align
(
    const memory::address_type address
) noexcept
{
    return sys::memory::page_align
    (
        address, 
        sys::memory::page_size()
    );
}

memory::size_type
inline address_space_limit() 
{
    using space_limit = struct sys::rlimit;

    space_limit limit;   
    if (getrlimit(RLIMIT_AS, &limit) == 0) 
    {
        if (limit.rlim_cur == RLIM_INFINITY) 
            return std::numeric_limits<memory::size_type>::max();

        return static_cast<memory::size_type>(limit.rlim_cur);
    }

    throw std::runtime_error("ERROR: Cannot query virtual address space limit");
}

} // memory namespace

} // system namespace
