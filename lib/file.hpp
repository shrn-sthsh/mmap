#pragma once

namespace sys
{

#include <sys/file.h>

#include <fcntl.h>
#include <unistd.h>

namespace file
{

using sint_t =   signed int;
using uint_t = unsigned int;

using descriptor  = sint_t;
using flag_code   = sint_t;
using status_code = sint_t;

inline auto &open   = sys::open;
inline auto &lock   = sys::flock;
inline auto &unlock = sys::flock;
inline auto &resize = sys::ftruncate;
inline auto &close  = sys::close;

} // memory namespace

} // system namespace
