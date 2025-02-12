#include "standard_allocator.hpp"

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include "util/record.hpp"


/**
 *  \fn Standard Allocator: Constructor
 *  
 *  Constructor does nothing as part of stateless nature of allocator. 
 *  The intended use is to call allocate and deallocate as the primary 
 *  mechanisms of allocating and deallocating chunks.
 */
template <typename T>
mmap::core::std::allocator<T>::allocator() noexcept
{}


/**
 *  \fn Standard Allocator: Destructor
 *
 *  Similar to the constructor, does nothing as part of stateless nature for 
 *  the same aforementioned reasons.  Again, look to allocate and deallocate.
 */
template <typename T>
mmap::core::std::allocator<T>::~allocator() noexcept
{}


/**
 *  \fn Standard Allocator: Copy Constructor and Assignment
 *  
 *  Similar to constructor, does nothing once again; no state to copy.
 *
 *  \param other:  l-value reference to another allocator (copy argument)
 */
template <typename T>
template <typename U>
mmap::core::std::allocator<T>::allocator
(
    const mmap::core::std::allocator<U> &other
) noexcept
{}

template <typename T>
template <typename U>
typename mmap::core::std::allocator<T> &
mmap::core::std::allocator<T>::operator=
(
    const mmap::core::std::allocator<U> &other
) noexcept 
{
    return *this;
}


/**
 *  \fn Standard Allocator: Move Constructor and Assignment
 *  
 *  Similar to constructor, does nothing once again; no state to move.
 *
 *  \param other:  r-value reference to another allocator (move argument)
 */
template <typename T>
template <typename U>
mmap::core::std::allocator<T>::allocator
(
    mmap::core::std::allocator<U> &&other
) noexcept
{}

template <typename T>
template <typename U>
typename mmap::core::std::allocator<T> &
mmap::core::std::allocator<T>::operator=
(
    mmap::core::std::allocator<U> &&other
) noexcept
{
    return *this;
}


/**
 *  \fn Standard Allocator: Allocator
 *  
 *  Allocates memory with a mapping based on the provided or default file 
 *  descriptor, flags, and protocols provided as arguments to the constructor.  
 *  Note, by default, an anonymous chunk of the provided capcity is created.
 *
 *  To create a shared chunk, create and map a chunk to a file or a 
 *  device, or more, the appropriate file descriptor, flags, and protocols 
 *  must be provieded as arguments to override the defaults.
 *
 *  Also, the provieded arguments are not checked in this user space routine in 
 *  an effort to maintain a similar level of performance to direct system calls 
 *  the.  The kernel will check and an exception is thrown on failure.
 *
 *  \param data_capacity:    capacity of the T type items (not in bytes)
 *  \param mapping_flags:    indicators to type of mapping
 *                           def: PRIVATE and ANONYMOUS (unmapped chunk)
 *  \param mapping_protocol: access permissions and memory growth pattern
 *                           def: READ and WRITE
 *  \param hint_address:     base adddress for mmap to use as hint or base
 *                           def: nullptr (let kernel chose page-aligned)
 *  \param file_descriptor:  optional file descriptor for other functions
 *                           def: invalid (-1) for anonymous mapping
 *  \param file_offset:      offset into the file (in bytes)
 *                           def: 0 bytes
 */
template <typename T>
typename mmap::core::std::allocator<T>::return_type
mmap::core::std::allocator<T>::allocate
(
    const mmap::core::std::allocator<T>::size_type  data_capacity, 
    const sys::memory::flag_code                        mapping_flags,
    const sys::memory::flag_code                        mapping_protocol,
    const sys::memory::address_type                     hint_address,
    const sys::file::descriptor_type                   &file_descriptor,
    const sys::file::size_type                          file_offset
)
{
    using data_type = T;

    // validate capacity
    const mmap::core::std::allocator<data_type>::size_type 
    memory_capacity = data_capacity * sizeof(data_type);

    if (memory_capacity <= sys::memory::ZERO_SPACE)
    {
        util::log::error<::std::invalid_argument>
        (
            "Argument to allocator must be a nonzero integer representing "
            "capacity of allocation as a number of template argument instances",
            util::log::type::ERROR
        );    
    }

    // ensure file is of sufficent size
    if (file_descriptor != sys::file::INVALID_FILE)
    {
        const sys::memory::status_code 
        resize_status = sys::file::resize
        (
            file_descriptor,
            memory_capacity
        );

        if (resize_status == mmap::INTERNAL_ERROR_CODE)
        {
            util::log::error<::std::bad_alloc>
            (
                ::std::format
                (
                    "Allocator unable to ensure required size of backing"
                    "file as indicated by requested size of {} bytes",
                    memory_capacity
                ),
                util::log::type::ERROR
            );
        }
    }

    // make new allocation
    pointer_type 
    memory_pointer = this->__allocator__
    (
        memory_capacity,
        hint_address,
        file_descriptor,
        file_offset,
        mapping_protocol,
        mapping_flags
    );

    return static_cast<return_type>(memory_pointer);
}


/**
 *  \fn Standard Allocator: Deallocator
 *  
 *  Deallocates memory mappings already allocated by allocate routine.  Given 
 *  allocators assume raw pointer safety is the caller's repsonsibility, the
 *  deallocator will make unmapping call with only rudementary checks.
 *
 *  \param memory_address: address of start of already allocated mapping
 *  \param data_capacity:  capacity of the T type items (not in bytes)
 */
template<typename T>
void 
mmap::core::std::allocator<T>::deallocate
(
    const mmap::core::std::allocator<T>::pointer_type memory_address,
    const mmap::core::std::allocator<T>::size_type    data_capacity
) noexcept
{
    using data_type = T;

    // validate capacity
    const mmap::core::std::allocator<data_type>::size_type 
    memory_capacity = data_capacity * sizeof(data_type);

    if (memory_capacity <= sys::memory::ZERO_SPACE)
    {
        util::log::error<::std::invalid_argument>
        (
            "Argument to dellocator must be a nonzero integer representing "
            "capacity of allocation as a number of template argument instances",
            util::log::type::ERROR
        ); 
    }

    // deallocate mapping 
    this->__deallocator__
    (
        memory_capacity, 
        memory_capacity
    );
}


/**
 *  \fn Standard Allocator: In-Place Object Constructor
 *  
 *  Object of type U will be constructed at the exact address provided with the
 *  appropriate argumenty list.
 *
 *  \param address:  address of exact spot in chunk to construct object
 *  \param argument: arguments to objects constructor
 */
template <typename T>
template <typename U, typename... parameters>
void 
mmap::core::std::allocator<T>::construct
(
    const U              *address, 
    const parameters &&...arguments
) 
{
    ::new (static_cast<pointer_type>(address)) U
    (
        ::std::forward<parameters>(arguments)...
    );
}

/**
 *  \fn Standard Allocator: In-Place Object Destructor
 *  
 *  Object of type U will be destructed at the exact address provided.
 *
 *  \param address:  address of exact spot in chunk to construct object
 */
template <typename U>
void 
destroy
(
    U *address
) noexcept 
{
    if (address)
        address->~U();
}


/**
 *  \fn Standard Allocator: Max Size / Capacity
 *  
 *  Object of type U will be destructed at the exact address provided.
 *
 *  \param address:  address of exact spot in chunk to construct object
 */
template <typename T>
typename mmap::core::std::allocator<T>::size_type
inline mmap::core::std::allocator<T>::capacity() 
const noexcept
{
    return static_cast<mmap::core::std::allocator<T>::size_type>
    (
        sys::memory::address_space_limit()
    );
}


/**
 *  \fn Standard Allocator: Address of References
 *  
 *  Object of type U will be destructed at the exact address provided.
 *
 *  \param address:  address of exact spot in chunk to construct object
 */
template <typename T>
typename mmap::core::std::allocator<T>::return_type
mmap::core::std::allocator<T>::address
(
    T &instance
) const noexcept 
{
    return &instance;
}

template <typename T>
const typename mmap::core::std::allocator<T>::return_type
mmap::core::std::allocator<T>::address
(
    const T &instance
) const noexcept 
{
    return &instance;
}


/**
 *  \fn Standard Allocator: Internal Allocator
 *  
 *  Internal allocator for chunk allocation and mapping.  Wraps mmap system 
 *  call to kernel.
 *
 *  \note Class level access only.
 *
 *  \param memory_size:      size of chunk in bytes
 *  \param mapping_flags:    indicators to type of mapping
 *  \param mapping_protocol: access permissions and memory growth pattern
 *  \param hint_address:     base adddress for mmap to use as hint or base
 *  \param file_descriptor:  optional file descriptor for other functions
 *  \param file_offset:      offset into the file (in bytes)
 *
 *  \ret   memory address:   a raw pointer to the allocated chunk on sucess
 *                           and MAP_FAILED on failure
 */
template <typename T>
typename mmap::core::std::allocator<T>::return_type
inline mmap::core::std::allocator<T>::__allocator__
(
    const sys::memory::size_type      memory_size, 
    const sys::memory::flag_code      mapping_flags,
    const sys::memory::flag_code      mapping_protocol,
    const sys::memory::address_type   hint_address,
    const sys::file::descriptor_type &file_descriptor,
    const sys::file::size_type        file_offset
)
{
    // make mapping syscall
    const sys::memory::address_type 
    memory_address = sys::memory::map
    (
        hint_address,
        memory_size,
        mapping_protocol,
        mapping_flags,
        file_descriptor,
        file_offset
    );

    if (memory_address == MAP_FAILED)
    {
        util::log::error<::std::bad_alloc>
        (
            "Kernel failed to allocate and map memory requested chunk",
            util::log::type::ERROR
        );
    }

    return memory_address;
}


/**
 *  \fn Standard Allocator: Internal Deallocator
 *  
 *  Internal deallocator for chunk dellocation and unmapping.  Wraps munmap
 *  system call to kernel.  
 *
 *  \note Class level access only.
 *
 *  \param memory_address: memory adddress for already allocated chunk
 *  \param memory_size:    size of chunk in bytes
 */
template <typename T>
void 
inline mmap::core::std::allocator<T>::__deallocator__
(
    sys::memory::address_type memory_address,
    sys::memory::size_type    memory_size
)
{
    // make unmapping syscall
    const sys::memory::status_code 
    unmap_status = sys::memory::unmap
    (
        memory_address,
        memory_size
    );

    if (unmap_status == mmap::INTERNAL_ERROR_CODE)
    {
        util::log::error<::std::bad_alloc>
        (
            "Kernel failed to unmap and deallocate memory requested chunk",
            util::log::type::ERROR
        );
    }
}
