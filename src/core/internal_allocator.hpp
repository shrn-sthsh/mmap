#pragma once

#include <lib/file.hpp>
#include <lib/mmap.hpp>


namespace mmap 
{

constexpr sint_t GLOBAL_SUCCESS_CODE = EXIT_SUCCESS;
constexpr sint_t EXTERNAL_ERROR_CODE = EXIT_FAILURE;
constexpr sint_t INTERNAL_ERROR_CODE = -1;

namespace core
{

sys::memory::address_type
inline allocate 
(
    const sys::memory::size_type      memory_capacity, 
    const sys::memory::flag_code      mapping_flags,
    const sys::memory::flag_code      mapping_protocol,
    const sys::memory::address_type   hint_address,
    const sys::file::descriptor_type &file_descriptor,
    const sys::file::size_type        file_offset
);


sys::memory::address_type
inline reallocate 
(
    const sys::memory::address_type memory_address,
    const sys::memory::size_type    memory_capacity,
    const sys::memory::size_type    newset_capacity,
    const sys::memory::flag_code    remapping_flags 
);


void 
inline deallocate 
(
    sys::memory::address_type memory_address,
    sys::memory::size_type    memory_capacity
);

} // core namepace

} // mmap namepace
