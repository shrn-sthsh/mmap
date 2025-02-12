#pragma once

#include <cstddef>
#include <iterator>
#include <string>

#include "lib/type.hpp"


namespace sys
{

#include <sys/file.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

using ::close;
using ::ftruncate;


namespace file
{

using descriptor_type = sint_t;
using size_type       = std::size_t;
using flag_code       = sint_t;
using status_code     = sint_t;

// file constants
constexpr size_type ZERO_SPACE  = size_type{0};
constexpr size_type ZERO_OFFSET = size_type{0};

// codes
constexpr descriptor_type   VALID_FILE = descriptor_type{1};
constexpr descriptor_type INVALID_FILE = descriptor_type{-1};

// management
inline auto &open  = ::sys::open;
inline auto &close = ::sys::close;

// synchronization
inline auto &lock   = ::sys::flock;
inline auto &unlock = ::sys::flock;

// sizing
inline auto &resize = ::sys::ftruncate;

// status
using metadata_type   = struct ::sys::stat;
inline auto &metadata = ::sys::fstat;

size_type
inline size(const descriptor_type &descriptor)
{
    metadata_type metadata;
    ::sys::file::metadata(descriptor, &metadata);

    return (metadata.st_size > ZERO_SPACE) 
        ? static_cast<size_type>(metadata.st_size)
        : ZERO_SPACE;
}

bool 
inline regular(const descriptor_type &descriptor) 
{ 
    metadata_type metadata;
    ::sys::file::metadata(descriptor, &metadata);

    return static_cast<bool>(S_ISREG(metadata.st_mode));
}

bool 
inline directory(const descriptor_type &descriptor) 
{ 
    metadata_type metadata;
    ::sys::file::metadata(descriptor, &metadata);

    return static_cast<bool>(S_ISDIR(metadata.st_mode));
}

bool 
inline character_device(const descriptor_type &descriptor) 
{ 
    metadata_type metadata;
    ::sys::file::metadata(descriptor, &metadata);

    return static_cast<bool>(S_ISCHR(metadata.st_mode));
}

bool 
inline block_device(const descriptor_type &descriptor) 
{ 
    metadata_type metadata;
    ::sys::file::metadata(descriptor, &metadata);

    return static_cast<bool>(S_ISBLK(metadata.st_mode));
}

bool 
inline pipe(const descriptor_type &descriptor) 
{ 
    metadata_type metadata;
    ::sys::file::metadata(descriptor, &metadata);

    return static_cast<bool>(S_ISFIFO(metadata.st_mode));
}

bool 
inline symbolic_link(const descriptor_type &descriptor) 
{ 
    metadata_type metadata;
    ::sys::file::metadata(descriptor, &metadata);

    return static_cast<bool>(S_ISLNK(metadata.st_mode));
}

bool 
inline socket(const descriptor_type &descriptor) 
{ 
    metadata_type metadata;
    ::sys::file::metadata(descriptor, &metadata);

    return static_cast<bool>(S_ISSOCK(metadata.st_mode));
}

} // file namespace

} // ::system namespace
