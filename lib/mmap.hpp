#pragma once

namespace sys
{

#include <sys/mman.h>

namespace memory
{

using status_code = signed int;
using flag_code   = signed int;

// mapping
inline auto &map   = mmap;
inline auto &unmap = munmap;
inline auto &remap = mremap;

// synchronicity & protection
inline auto &sync    = msync;
inline auto &lock    = mlock;
inline auto &unlock  = munlock;
inline auto &protect = mprotect;

// utility
inline auto &advise   = madvise;
inline auto &resident = mincore;

namespace shared
{

inline auto &open  = shm_open;
inline auto &close = shm_unlink;

} // shared namespace

} // memory namespace

} // system namespace
