#pragma once 


#include <cstddef>
#include <cstdlib>
#include <memory>

#include <lib/file.hpp>
#include <lib/mmap.hpp>


namespace mmap 
{

constexpr sint_t GLOBAL_SUCCESS_CODE = EXIT_SUCCESS;
constexpr sint_t EXTERNAL_ERROR_CODE = EXIT_FAILURE;
constexpr sint_t INTERNAL_ERROR_CODE = -1;

namespace core
{

namespace std 
{
    
/**
 *  \class template Standard Memory Mapping Allocator
 *  \brief STL-complaint allocator based on mmap family of system calls
 *
 *  For UNIX-like systems, the mmap system call is a generic memory mapper
 *  and allocator which can be used to create shared chunks, mapped chunks 
 *  (into a device or a file), or anonymous chunks of memory. 
 *
 *  This allocator is an STL compliant allocator which allows for most the 
 *  functionality provieded by the usage of the mmap family of system calls,
 *  both naturally in it's generic usage and through raw address exposure.
 *
 *  That's to say, mmap, munmap, and mremap are used in allocation, dealloc-
 *  ation, and reallocation, respectively, with most of the options available.
 *  Other calls like msync can be run on the safely exposed memory address.
 *
 *  This effectively applys the advatages presented in C++-style programming
 *  to the C-style system call APIs provieded by UNIX-like systems such as 
 *  Linux, BSD, and macOS -- all of which this allocator is compliant with.
 *
 *  Note, for use cases such as mapping a normal file to a chunk, mapping a 
 *  device to a chunk, or creating an inter-process shared chunk, the 
 *  appropriate file descriptor should be created and provieded with flags.
 */
template <typename T>
class allocator 
{
public:
    using data_type    = T;
    using return_type  = data_type *;
    using pointer_type = void *;
    using size_type    = ::std::size_t;

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


    // allocation and deallocation
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

    void 
    deallocate
    (
        const pointer_type memory_address,
        const size_type    data_capacity
    ) noexcept;


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


// Internal allocation management 
private:
    return_type 
    inline __allocator__
    (
        const size_type                   memory_capacity, 
        const sys::memory::flag_code      mapping_flags    = MAP_PRIVATE | MAP_ANONYMOUS,
        const sys::memory::flag_code      mapping_protocol = PROT_READ | PROT_WRITE,
        const sys::memory::address_type   hint_address     = sys::memory::DEFAULT_BASE,
        const sys::file::descriptor_type &file_descriptor  = sys::file::INVALID_FILE,
        const sys::file::size_type        file_offset      = sys::file::ZERO_OFFSET
    );

    void 
    inline __deallocator__
    (
        const sys::memory::address_type memory_address,
        const sys::memory::size_type    memory_capacity
    );
};

} // standard namespace

} // core namespace

} // mmap namespace
