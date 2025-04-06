#include "util/record.hpp"

#include "internal_allocator.hpp"


/**
 *  \fn Internal Allocator
 *  
 *  Allocator for chunk allocation and mapping.  Wraps mmap system call to kernel.
 *
 *  \note Class level access only.
 *
 *  \param memory_capacity:  size of chunk in bytes
 *  \param mapping_flags:    indicators to type of mapping
 *  \param mapping_protocol: access permissions and memory growth pattern
 *  \param hint_address:     base adddress for mmap to use as hint or real base
 *  \param file_descriptor:  optional file descriptor for other functions
 *  \param file_offset:      offset into the file (in bytes)
 *
 *  \ret   memory address:   a raw pointer to the allocated chunk on sucess
 *                           and MAP_FAILED on failure
 */
template <typename T>
sys::memory::address_type
inline mmap::core::allocate
(
    const sys::memory::size_type      memory_capacity, 
    const sys::memory::flag_code      mapping_flags,
    const sys::memory::flag_code      mapping_protocol,
    const sys::memory::address_type   hint_address,
    const sys::file::descriptor_type &file_descriptor,
    const sys::file::size_type        file_offset
)
{
    // make mapping system call
    const sys::memory::address_type 
    memory_address = sys::memory::map
    (
        hint_address,
        memory_capacity,
        mapping_protocol,
        mapping_flags,
        file_descriptor,
        file_offset
    );

    if (memory_address == MAP_FAILED)
    {
        util::log::error<std::bad_alloc>
        (
            "Kernel failed to allocate and map memory requested chunk",
            util::log::type::ERROR
        );
    }

    return memory_address;
}


/**
 *  \fn Internal Reallocator
 *  
 *  Reallocator for chunk rellocation and remapping.  Wraps mremap system call 
 *  to kernel.  
 *
 *  \note Class level access only.
 *
 *  \param memory_address:  memory adddress for already allocated chunk
 *  \param memory_capacity: size of current chunk in bytes
 *  \param newset_capacity: size of new chunk in bytes
 *  \param remapping_flags: indicators for the type of remapping
 */
template <typename T>
sys::memory::address_type
inline mmap::core::reallocate
(
    const sys::memory::address_type memory_address,
    const sys::memory::size_type    memory_capacity,
    const sys::memory::size_type    newset_capacity,
    const sys::memory::flag_code    remapping_flags 
)
{
    // make remapping system call
    const sys::memory::address_type
    remapped_address = sys::memory::remap
    (
        memory_address,
        memory_capacity,
        newset_capacity,
        remapping_flags
    );

    if (remapped_address == MAP_FAILED)
    {
        util::log::error<std::bad_alloc>
        (
            "Kernel failed to reallocate and remap memory existing chunk",
            util::log::type::ERROR
        );
    }

    return remapped_address;
}


/**
 *  \fn Internal Deallocator
 *  
 *  Deallocator for chunk dellocation and unmapping.  Wraps munmap system call 
 *  to kernel.  
 *
 *  \note Class level access only.
 *
 *  \param memory_address: memory adddress for already allocated chunk
 *  \param memory_size:    size of chunk in bytes
 */
template <typename T>
void 
inline mmap::core::deallocate
(
    sys::memory::address_type memory_address,
    sys::memory::size_type    memory_capacity
)
{
    // make unmapping system call
    const sys::memory::status_code 
    unmap_status = sys::memory::unmap
    (
        memory_address,
        memory_capacity
    );

    if (unmap_status == mmap::INTERNAL_ERROR_CODE)
    {
        util::log::error<std::bad_alloc>
        (
            "Kernel failed to unmap and deallocate existing memory chunk",
            util::log::type::ERROR
        );
    }
}
