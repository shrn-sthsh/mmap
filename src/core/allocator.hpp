#pragma once 


#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include <boost/unordered/unordered_flat_map_fwd.hpp>

#include <lib/file.hpp>
#include <lib/mmap.hpp>


namespace mmap 
{

constexpr sint_t GLOBAL_SUCCESS_CODE = EXIT_SUCCESS;
constexpr sint_t EXTERNAL_ERROR_CODE = EXIT_FAILURE;
constexpr sint_t INTERNAL_ERROR_CODE = sint_t{-1};

namespace core
{

// stateful memory mapping calls
template <typename function_type>
concept stateful = std::same_as<function_type, decltype(sys::memory::protect)>
                || std::same_as<function_type, decltype(sys::memory::advise)>
                || std::same_as<function_type, decltype(sys::memory::repage)>;


/**
 *  \class Stateful Memory Mapping Allocator
 *  \brief a memory mapping allocator which holds RAII handles on allocations
 *
 *  For UNIX-like systems, the mmap system call is a generic memory mapper
 *  and allocator which can be used to create shared chunks, mapped chunks 
 *  (into a device or a file), or anonymous chunks of memory. 
 *
 *  This allocator is a stateful allocator which allows for most of the 
 *  functionality provided by the usage of the mmap family of system calls while
 *  providing automatic allocation management.
 *
 *  That's to say, mmap, munmap, and mremap are used in allocation, dealloc-
 *  ation, and reallocation, respectively, with most of the options available.
 *  Other calls like msync can be run on the safely through executors.
 *
 *  The allocator is able to manage the state of many allocations through the 
 *  use of internal structures which may vary in performance depending on the 
 *  number of allocations made and management by the allocator at once.
 *
 *  If a usage of the allocator exists with context of the relative number of 
 *  allocations made, this context is useful for the allocator in optimizing 
 *  performance for the use case, and can be provided to the constructor.
 *
 *  Note, for use cases such as mapping a normal file to a chunk, mapping a 
 *  device to a chunk, or creating an inter-process shared chunk, the 
 *  appropriate file descriptor should be created and provided with flags.
 */
template <typename T>
class allocator 
{
public:
    using data_type = T;
    using size_type = std::size_t;

    using handle_type  = std::weak_ptr<data_type []>;
    using pointer_type = std::shared_ptr<std::byte []>;

    // rebind allocator to another type U
    template <typename U>
    struct rebind 
    {
        using other = allocator<U>;
    };


    // constructors
    allocator
    (
        const size_type size = size_type{1}
    ) noexcept;
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


    // external allocation, reallocation, deallocation
    handle_type
    allocate
    (
        const size_type                   data_capacity, 
        const sys::memory::flag_code      mapping_flags    = MAP_PRIVATE | MAP_ANONYMOUS,
        const sys::memory::flag_code      mapping_protocol = PROT_READ | PROT_WRITE,
        const sys::memory::address_type   base_address     = sys::memory::DEFAULT_BASE,
        const sys::file::descriptor_type &file_descriptor  = sys::file::INVALID_FILE,
        const sys::file::size_type        file_offset      = sys::file::ZERO_OFFSET
    );

    handle_type
    reallocate
    (
        const handle_type            &memory_handle,
        const size_type               data_capacity,
        const sys::memory::flag_code  remapping_flags = MREMAP_MAYMOVE
    );

    void 
    deallocate
    (
        const handle_type &memory_handle
    );


    // external calls to m-family system calls through executors
    template <typename function_type, typename... argument_list>
    requires sys::memory::auxiliary<function_type> 
          || sys::memory::aggregate<function_type>
    auto execute
    (
        function_type      function, 
        argument_list &&...arguments,
        handle_type       &handle
    ) -> decltype(function(std::forward<argument_list>(arguments)...));

    template 
    <
        typename    return_type, 
        class       class_type, 
        typename... argument_list, 
        typename    object_type
    >
    requires sys::memory::auxiliary<return_type (class_type::*)(argument_list...)> 
          || sys::memory::aggregate<return_type (class_type::*)(argument_list...)>
    auto execute
    (
        return_type (class_type::*function)(argument_list...),
        object_type             &&object,
        argument_list        &&...arguments,
        handle_type              &handle
    ) -> decltype
    (
        (std::forward<object_type>(object).*function)
        (std::forward<argument_list>(arguments)...)
    );

    template 
    <
        typename    return_type, 
        class       class_type, 
        typename... argument_list, 
        typename    object_type
    >
    requires sys::memory::auxiliary<return_type (class_type::*)(argument_list...)> 
          || sys::memory::aggregate<return_type (class_type::*)(argument_list...)>
    auto execute
    (
        return_type (class_type::*function)(argument_list...) const,
        object_type             &&object,
        argument_list        &&...arguments,
        handle_type              &handle
    ) -> decltype
    (
        (std::forward<object_type>(object).*function)
        (std::forward<argument_list>(arguments)...)
    );


    // construction and destruction of objects
    template <typename U, typename... argument_list>
    void 
    construct
    (
        handle_type       &handle, 
        argument_list &&...arguments
    );

    template <typename U>
    void 
    destroy
    (
        handle_type &handle
    );


    // addresses
    handle_type 
    address
    (
        data_type &instance
    ) const noexcept;

    const handle_type 
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


// Allocation metadata storage
protected:

    struct alignas(64) metadata_type 
    {
        // per allocation metadata
        pointer_type                memory_pointer;
        sys::memory::size_type      memory_capacity;
        sys::memory::flag_code      mapping_flags;
        sys::memory::flag_code      mapping_protocol;
        sys::memory::flag_code      remapping_flags;
        sys::memory::flag_code      paging_advice;
        sys::file::descriptor_type &file_descriptor;
        sys::file::size_type        file_offset;


        // constructors
        template <typename U>
        metadata_type
        (
            const typename allocator<U>::metadata_type &other
        ) noexcept;

        template <typename U>
        metadata_type
        (
            typename allocator<U>::metadata_type &&other
        ) noexcept;


        // assignments
        template <typename U>
        metadata_type &
        operator=
        (
            const typename allocator<U>::metadata_type &other
        ) noexcept;

        template <typename U>
        metadata_type &
        operator=
        (
            typename allocator<U>::metadata_type &&other
        ) noexcept;
    };


// Internal allocation management 
private:

    /**
     *  Threshold to switch from dynamic array to hashtable
     *
     *  Size of metadata is likely less than or equal to 64 bytes (common cache 
     *  line size on every machine), thus 32 Kb (L1 data cache size) / 64 = 1 Kb
     *  entries would maximize usage under the L1 cache.  Anything greater
     *  than this, a hash table is likely to do better.
     */
    static constexpr size_type 
    CACHE_AWARE_THRESHOLD = 32 * 1'024 / sizeof(metadata_type);
    
    // allocation metadata store
    using array = std::vector<metadata_type>;
    using table = boost::unordered_flat_map
    <
        std::weak_ptr<data_type []>, 
        metadata_type, 
        std::owner_less<std::weak_ptr<data_type []>>
    >;

    bool default_storage = true;
    std::variant<array, table> storage;

           
    // internal allocator
    handle_type 
    inline __allocator__
    (
        const size_type                   memory_capacity, 
        const sys::memory::flag_code      mapping_flags    = MAP_PRIVATE | MAP_ANONYMOUS,
        const sys::memory::flag_code      mapping_protocol = PROT_READ | PROT_WRITE,
        const sys::memory::address_type   hint_address     = sys::memory::DEFAULT_BASE,
        const sys::file::descriptor_type &file_descriptor  = sys::file::INVALID_FILE,
        const sys::file::size_type        file_offset      = sys::file::ZERO_OFFSET
    );

    // internal reallocator
    handle_type 
    inline __reallocator__ 
    (
        const sys::memory::address_type memory_address,
        const sys::memory::size_type    memory_capacity, 
        const sys::memory::size_type    memory_increase,
        const sys::memory::flag_code    remapping_flags = MREMAP_MAYMOVE
    );

    // internal deallocator
    void 
    inline __deallocator__
    (
        const sys::memory::address_type memory_address,
        const sys::memory::size_type    memory_capacity
    );
};

} // core namespace

} // mmap namespace
