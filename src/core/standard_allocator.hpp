#pragma once 


#include <cstddef>
#include <cstdlib>
#include <type_traits>

#include <lib/file.hpp>
#include <lib/mmap.hpp>

#include "internal_allocator.hpp"


namespace mmap 
{

namespace core
{

namespace std 
{
    
/**
 *  \class Standard Templated Memory Mapping Allocator
 *  \brief STL-complaint allocator based on mmap family of system calls
 *
 *  For UNIX-like systems, the mmap system call is a generic memory mapper
 *  and allocator which can be used to create shared chunks, mapped chunks 
 *  (into a device or a file), or anonymous chunks of memory. 
 *
 *  This allocator is an STL compliant allocator which allows for most the 
 *  functionality provided by the usage of the mmap family of system calls,
 *  both naturally in it's generic usage and through raw address exposure.
 *
 *  That's to say, mmap, munmap, and mremap are used in allocation, dealloc-
 *  ation, and reallocation, respectively, with most of the options available.
 *  Other calls like msync can be run on the safely exposed memory address.
 *
 *  This effectively applys the advatages presented in C++-style programming
 *  to the C-style system call APIs provided by UNIX-like systems such as 
 *  Linux, BSD, and macOS -- all of which this allocator is compliant with.
 *
 *  Note, for use cases such as mapping a normal file to a chunk, mapping a 
 *  device to a chunk, or creating an inter-process shared chunk, the 
 *  appropriate file descriptor should be created and provided with flags.
 */
template <typename T = ::std::byte>
class allocator 
{
public:
    using data_type    = T;
    using return_type  = data_type *;
    using pointer_type = void *;
    using size_type    = ::std::size_t;
    using diff_type    = ::std::make_signed_t<size_type>;

    // Rebind allocator to another type U
    template <typename U>
    struct rebind 
    {
        using other = allocator<U>;
    };


    // constructors
    allocator()  noexcept = default;
    ~allocator() noexcept = default;

    template <typename U>
    explicit allocator
    (
        const allocator<U> &other
    ) noexcept;
 
    template <typename U>
    explicit allocator
    (
        allocator<U> &&other
    ) noexcept;


    // assignments
    template <typename U>
    allocator 
    &operator=
    (
        const allocator<U> &other
    ) noexcept;

    template <typename U>
    allocator 
    &operator=
    (
        allocator<U> &&other
    ) noexcept;


    // external allocation, reallocation, and deallocation
    return_type
    allocate
    (
        const size_type                   data_capacity, 
        const sys::memory::flag_code      mapping_flags    = MAP_PRIVATE | MAP_ANONYMOUS,
        const sys::memory::flag_code      mapping_protocol = PROT_READ | PROT_WRITE,
        const sys::memory::address_type   base_address     = sys::memory::DEFAULT_BASE,
        const sys::file::descriptor_type &file_descriptor  = sys::file::INVALID_FILE,
        const sys::file::size_type        file_offset      = sys::file::ZERO_OFFSET
    );

    return_type
    reallocate
    (
        const pointer_type           memory_address,
        const size_type              curr_data_capacity,
        const size_type              next_data_capacity,
        const sys::memory::flag_code remapping_flags
    );

    return_type
    reallocate
    (
        const pointer_type           memory_address,
        const size_type              data_capacity,
        const diff_type              move_capacity,
        const sys::memory::flag_code remapping_flags
    );

    void 
    deallocate
    (
        const pointer_type memory_address,
        const size_type    data_capacity
    );


    // in-allocation construct and destruct
    template <typename U, typename... parameters>
    void 
    construct
    (
        const U              *address, 
        const parameters &&...arguments
    );

    template <typename U>
    void 
    destroy
    (
        const U *address
    );


    // addresses
    return_type 
    address
    (
        data_type &instance
    ) const noexcept;

    const return_type 
    address
    (
        const data_type &instance
    ) const noexcept;


    // metadata
    size_type 
    inline capacity() const noexcept;
    using max_size = decltype(&allocator::capacity);


    // equality operators
    bool 
    operator==
    (
        const allocator &other
    ) const noexcept;
    bool 
    operator!=
    (
        const allocator &other
    ) const noexcept;

private:
    static constexpr auto &__allocate__   = mmap::core::allocate;
    static constexpr auto &__reallocate__ = mmap::core::reallocate;
    static constexpr auto &__deallocate__ = mmap::core::deallocate;
};

} // standard namespace

} // core namespace

} // mmap namespace
