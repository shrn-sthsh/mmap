#include "allocator.hpp"

#include <concepts>
#include <format>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include "lib/mmap.hpp"
#include "util/record.hpp"


/**
 *  \fn Stateful Allocator: Constructor
 *  
 *  Constructor requires no arguments, but can accept a nonzero integer argument 
 *  for a known number of allocation calls to be made.  This provdes useful
 *  context as the number of allocations can change the which internal structure
 *  to use in storing allocation metadata, heavily influencing performance.
 *
 *  \param capacity: expected maximum number of allocations made by allocator 
 *                   note: more allocations that this number can be made,
 *                         but a better estimate is useful to the allocator
 */
template <typename T>
mmap::core::allocator<T>::allocator
(
    const mmap::core::allocator<T>::size_type capacity
) noexcept
{
    using data_type = T;

    // capacity indicates array provides better performance
    using allocator = mmap::core::allocator<data_type>;
    if (capacity < allocator::CACHE_AWARE_THRESHOLD)
    {
        using array = mmap::core::allocator<data_type>::array;

        this->storage         = array{};
        this->default_storage = true;

        std::get<array>(this->storage).reserve(capacity); 
    }

    // capacity indicates hashtable provides better performance
    else 
    {
        using table = mmap::core::allocator<data_type>::table; 

        this->storage         = table{};
        this->default_storage = false;

        std::get<table>(this->storage).rehash(capacity); 
    }
}


/**
 *  \fn Stateful Allocator: Allocator
 *  
 *  Allocates memory with a mapping based on the provided or default file 
 *  descriptor, flags, and protocols provided as arguments to the allocator.  
 *  Note, by default, an anonymous chunk of the provided capcity is created.
 *
 *  To create a shared chunk, create and map a chunk to a file or a 
 *  device, or more, the appropriate file descriptor, flags, and protocols 
 *  must be provided as arguments to override the defaults.
 *
 *  Also, the provided arguments are minmally checked in this user space routine 
 *  in an effort to maintain a low level of latency and overhead above the base 
 *  system calls.  The kernel will check, and an exception is thrown on failure.
 *
 *  \param data_capacity:    capacity of the T type items (not in bytes)
 *  \param mapping_flags:    indicators to type of mapping
 *                           def: PRIVATE and ANONYMOUS (unmapped chunk)
 *  \param mapping_protocol: access permissions and memory growth pattern
 *                           def: READ and WRITE
 *  \param hint_address:     base adddress for mmap to use as hint or real base
 *                           def: nullptr (let kernel chose page-aligned)
 *  \param file_descriptor:  optional file descriptor for other functions
 *                           def: invalid (-1) for anonymous mapping
 *  \param file_offset:      offset into the file (in bytes)
 *                           def: 0 bytes
 *
 *  \ret   memory_handle:    a weak reference to a memory allocation which can
 *                           be used as a key for other routines
 */
template <typename T>
typename mmap::core::allocator<T>::handle_type
mmap::core::allocator<T>::allocate
(
    const mmap::core::allocator<T>::size_type  data_capacity, 
    const sys::memory::flag_code               mapping_flags,
    const sys::memory::flag_code               mapping_protocol,
    const sys::memory::address_type            hint_address,
    const sys::file::descriptor_type          &file_descriptor,
    const sys::file::size_type                 file_offset
)
{
    using data_type = T;

    // validate capacity
    const auto
    memory_capacity = data_capacity * sizeof(data_type);

    if (memory_capacity <= sys::memory::ZERO_SPACE)
    {
        util::log::error<std::invalid_argument>
        (
            "Argument to allocator must be a nonzero integer representing "
            "capacity of allocation as a number of template argument instances",
            util::log::type::ERROR
        );
    }

    // ensure file is of sufficent size
    if (file_descriptor != sys::file::INVALID_FILE)
    {
        const auto
        file_size = sys::file::size(file_descriptor);

        // resize file if not correct size
        if (file_size < memory_capacity)
        {
            const sys::memory::status_code 
            resize_status = sys::file::resize
            (
                file_descriptor,
                memory_capacity
            );

            if (resize_status == mmap::INTERNAL_ERROR_CODE)
            {
                util::log::error<std::bad_alloc>
                (
                    std::format
                    (
                        "Allocator unable to ensure required size of backing"
                        "file as indicated by requested size of {} bytes",
                        memory_capacity
                    ),
                    util::log::type::ERROR
                );
            }
        }
    }

    // make new allocation
    const auto 
    memory_pointer = mmap::core::allocator<data_type>::pointer_type
    (
        this->__allocator__
        (
            memory_capacity,
            mapping_flags,
            mapping_protocol,
            hint_address,
            file_descriptor,
            file_offset
        ),
        this->__deallocator__
    );

    // save weak reference external key
    const auto
    key = mmap::core::allocator<data_type>::handle_type{memory_pointer};

    // create metadata
    const auto 
    metadata = mmap::core::allocator<data_type>::metadata_type 
    (
        std::move(memory_pointer),
        memory_capacity,
        mapping_flags,
        mapping_protocol,
        sys::memory::NO_FLAG,
        file_descriptor,
        file_offset
    );
    
    // save to storage
    if (this->default_storage)
        std::get<array>(this->storage).emplace_back(std::move(metadata));
    else
        std::get<table>(this->storage).emplace(key, std::move(metadata));

    return key;
}


/**
 *  \fn Stateful Allocator: Reallocator 
 *  
 *  Reallocates memory mappings already allocated by allocate routine.  The 
 *  provided memory handle will be checked if still valid before reallocation
 *  is done.  System arguments, however, will only be checked by the kernel, and
 *  an exception is thrown on failure.
 *
 *  \param memory_handle:   handle to to-be reallocated memory allocation
 *  \param data_capacity:   new capacity of the T type items (not in bytes)
 *  \param remapping_flags: indicators for the type of remapping
 *                          def: mapping MAY MOVE
 *
 *  \ret   memory_handle:    a weak reference to a memory allocation which can
 *                           be used as a key for other routines
 */
template<typename T>
mmap::core::allocator<T>::handle_type
mmap::core::allocator<T>::reallocate
(
    const mmap::core::allocator<T>::handle_type &memory_handle,
    const mmap::core::allocator<T>::size_type    data_capacity, 
    const sys::memory::flag_code                 remapping_flags
) 
{
    using data_type = T;

    // validate capacity
    const auto
    resized_memory_capacity = data_capacity * sizeof(data_type);

    if (resized_memory_capacity <= sys::memory::ZERO_SPACE)
    {
        util::log::error<std::invalid_argument>
        (
            "Argument (2) to rellocator must be a nonzero integer representing "
            "capacity of allocation as a number of template argument instances",
            util::log::type::ERROR
        );
    }

    // validate memory handle 
    if (memory_handle.expired())
    {
        util::log::error<std::runtime_error>
        (
            "Argument (1) to rellocator was an invalid weak reference to a"
            "possible allocation",
            util::log::type::FLAG
        );
    } 

    // array-based storage execution
    if (this->default_storage)
    { 
        // capture allocation's strong pointer
        const mmap::core::allocator<data_type>::pointer_type
        key = memory_handle.lock();

        // search for allocation's metadata by strong pointer as key
        using array = mmap::core::allocator<data_type>::array;
        for (const auto &metadata: std::get<array>(this->storage))
        {
            if (metadata.memory_pointer != key)
                continue;

            // reallocate and update metadata within data structure
            using address_type = sys::memory::address_type;

            const auto
            memory_pointer = mmap::core::allocator<data_type>::pointer_type
            (
                this->__reallocator__
                (
                    static_cast<address_type>(metadata.memory_pointer.get()),
                    metadata.memory_capacity,
                    resized_memory_capacity,
                    remapping_flags
                ),
                this->__deallocator__
            );
            
            metadata.memory_pointer   = std::move(memory_pointer);
            metadata.memory_capacity  = resized_memory_capacity;
            metadata.remapping_flags |= remapping_flags;

            const auto
            key = mmap::core::allocator<data_type>::handle_type{memory_pointer};

            return key;
        }
    }

    // table-based storage execution
    else
    {
        // weak pointer is table key
        const auto &key = memory_handle;

        // save table reference
        using table = mmap::core::allocator<data_type>::table;
        table &storage = std::get<table>(this->storage);

        // pull out key-value pair
        auto node = storage.extract(key);
        if (!node)
        {
            const mmap::core::allocator<data_type>::metadata_type 
            &metadata = node.mapped();

            // reallocate and update metadata
            using address_type = sys::memory::address_type;
            const auto
            memory_pointer = mmap::core::allocator<data_type>::pointer_type
            (
                this->__reallocator__
                (
                    static_cast<address_type>(metadata.memory_pointer.get()),
                    metadata.memory_capacity,
                    resized_memory_capacity,
                    remapping_flags
                ),
                this->__deallocator__
            );            
            
            metadata.memory_pointer   = std::move(memory_pointer);
            metadata.memory_capacity  = resized_memory_capacity;
            metadata.remapping_flags |= remapping_flags;

            // replace old pair with new
            const auto
            key = mmap::core::allocator<data_type>::handle_type{memory_pointer};
            storage.emplace(key, std::move(metadata));

            return key;
        }
    }

    // throw on no allocation metadata found
    util::log::error<std::runtime_error>
    (
        "Reallocator was unable reallocate possible allocation becuase "
        "no data on such an allocation exists internally",
        util::log::type::ERROR
    ); 
}


/**
 *  \fn Stateful Allocator: Deallocator
 *  
 *  Deallocates memory mappings already allocated by allocate routine.  The 
 *  provided memory handle will be checked if still valid before deallocation
 *  is done.  System arguments, however, will only be checked by the kernel, and
 *  an exception is thrown on failure.
 *
 *  \param memory_handle: handle to to-be deallocated allocation
 */
template<typename T>
void 
mmap::core::allocator<T>::deallocate
(
    const mmap::core::allocator<T>::handle_type &memory_handle
) 
{
    using data_type = T;

    // validate memory handle 
    if (memory_handle.expired())
    {
        util::log::error<std::runtime_error>
        (
            "Argument to dellocator was an invalid weak reference to a"
            "possible allocation",
            util::log::type::FLAG
        );
    } 

    // array-based storage execution
    if (this->default_storage)
    { 
        // capture allocation's strong pointer
        const mmap::core::allocator<data_type>::pointer_type
        key = memory_handle.lock();

        // search for allocation's metadata by strong pointer as key
        using array = mmap::core::allocator<data_type>::array;
        for (auto &metadata: std::get<array>(this->storage))
        {
            if (metadata.memory_pointer != key)
                continue;

            // deallocate allocation
            using address_type = sys::memory::address_type;
            this->__deallocator__
            (
                static_cast<address_type>(metadata.memory_pointer.get()),
                metadata.memory_capacity
            );

            // invalidate data instead of incurring cost of linear
            // shift from deletion to minimize deletion performance 
            metadata = std::move(metadata);

            return;
        }
    }

    // table-based storage execution
    else
    {
        // weak pointer is table key
        const auto &key = memory_handle;

        // save table reference
        using table = mmap::core::allocator<data_type>::table;
        table &storage = std::get<table>(this->storage);

        // pull out key-value pair
        auto node = storage.extract(key);
        if (!node)
        {
            const mmap::core::allocator<data_type>::metadata_type 
            &metadata = node.mapped();

            // deallocate allocation
            using address_type = sys::memory::address_type;
            this->__deallocator__
            (
                static_cast<address_type>(metadata.memory_pointer.get()),
                metadata.memory_capacity
            );

            return;
        }
    }

    // throw on no allocation metadata found
    util::log::error<std::runtime_error>
    (
        "Deallocator was unable deallocate possible allocation becuase "
        "no data on such an allocation exists internally",
        util::log::type::ERROR
    );
}


/**
 *  \fn Stateful Allocator: Auxiliary and Aggregate Routine Executor 
 *  
 *  Provides safely ensured execution of other m-family routines on an
 *  allocation provided it's memory handle.
 *
 *  \param function:  the function to operate on the allocation
 *  \param arguments: all arguments to to-be executed function except address
 *  \param handle:    handle who's allocation is a operable argument
 *
 *  \ret   status:    executed function's return value
 */
template <typename T>
template <typename function_type, typename... argument_list>
requires sys::memory::auxiliary<function_type> 
      || sys::memory::aggregate<function_type>
auto mmap::core::allocator<T>::execute
(
    function_type                              function, 
    argument_list                         &&...arguments,
    mmap::core::allocator<T>::handle_type     &handle
) -> decltype(function(std::forward<argument_list>(arguments)...))
{
    using data_type = T;

    // validate memory handle 
    if (handle.expired())
    {
        util::log::error<std::runtime_error>
        (
            "Argument to executor was an invalid weak reference to a possible "
            "allocation",
            util::log::type::FLAG
        );
    } 

    // array-based storage execution
    if (this->default_storage)
    { 
        // capture allocation's strong pointer
        const mmap::core::allocator<data_type>::pointer_type
        key = handle.lock();

        // search for allocation's metadata by strong pointer as key
        using array = mmap::core::allocator<data_type>::array;
        for (auto &metadata: std::get<array>(this->storage))
        {
            if (metadata.memory_pointer != key)
                continue;

            // auxiliary routine execution
            if constexpr (sys::memory::auxiliary<function_type>)
            {
                // stateful routines need to update metadata
                if constexpr 
                (std::same_as<function_type, decltype(sys::memory::protect)>)
                {
                    metadata.mapping_protocol 
                        |= std::get<2>(std::forward_as_tuple(arguments...));
                }

                else if constexpr
                (std::same_as<function_type, decltype(sys::memory::advise)>)
                {
                    metadata.paging_advice
                        |= std::get<2>(std::forward_as_tuple(arguments...));
                }

                // auxiliary routine execution
                return function
                (
                    metadata.memory_pointer.get(),
                    std::forward<argument_list>(arguments)...
                );
            }

            // aggregate routine execution
            else return function(std::forward<argument_list>(arguments)...);
        }
    }

    // table-based storage execution
    else
    {
        // weak pointer is table key
        const auto &key = handle;

        // save table reference
        using table = mmap::core::allocator<data_type>::table;
        table &storage = std::get<table>(this->storage);

        // auxiliary routine execution
        if constexpr (sys::memory::auxiliary<function_type>)
        {
            // stateful routines should update metadata 
            auto metadata = storage.find(key);
            if (metadata != storage.end())
            {
                // stateful routines need to update metadata
                if constexpr 
                (std::same_as<function_type, decltype(sys::memory::protect)>)
                {
                    metadata->mapping_protocol 
                        |= std::get<2>(std::forward_as_tuple(arguments...));
                }

                else if constexpr
                (std::same_as<function_type, decltype(sys::memory::advise)>)
                {
                    metadata->paging_advice
                        |= std::get<2>(std::forward_as_tuple(arguments...));
                }

                // auxiliary routine execution
                return function
                (
                    metadata->memory_pointer.get(),
                    std::forward<argument_list>(arguments)...
                );
            }

            else 
            {
                // throw on no allocation metadata found
                util::log::error<std::runtime_error>
                (
                    "Executor was unable execute function on possible allocation "
                    "becuase no data on such an allocation exists internally",
                    util::log::type::ERROR
                );
            }
        } 

        // aggregate routine execution
        else return function(std::forward<argument_list>(arguments)...);
    }

    // throw on no allocation metadata found
    util::log::error<std::runtime_error>
    (
        "Executor was unable execute function on possible allocation "
        "becuase no data on such an allocation exists internally",
        util::log::type::ERROR
    );
}


/**
 *  \fn Stateful Allocator: Auxiliary and Aggregate Class Routine Executor 
 *  
 *  Provides safely ensured execution of other m-family routines part of a class
 *  on an allocation provided it's memory handle.
 *
 *  \param function:  the function to operate on the allocation
 *  \param object:    the object who this executed function is a member of
 *  \param arguments: all arguments to to-be executed function except address
 *  \param handle:    handle who's allocation is a operable argument
 *
 *  \ret   status:    executed function's return value
 */
template <typename T>
template 
<
    typename    return_type, 
    class       class_type, 
    typename... argument_list, 
    typename    object_type
>
requires sys::memory::auxiliary<return_type (class_type::*)(argument_list...)> 
      || sys::memory::aggregate<return_type (class_type::*)(argument_list...)>
auto mmap::core::allocator<T>::execute
(
    return_type (class_type::*function)(argument_list...),
    object_type                              &&object, 
    argument_list                         &&...arguments,
    mmap::core::allocator<T>::handle_type     &handle
) -> decltype
(
    (std::forward<object_type>(object).*function)
    (std::forward<argument_list>(arguments)...)
)
{
    using data_type = T;

    // validate memory handle 
    if (handle.expired())
    {
        util::log::error<std::runtime_error>
        (
            "Argument to executor was an invalid weak reference to a possible "
            "allocation",
            util::log::type::FLAG
        );
    } 

    // array-based storage execution
    using function_type = return_type (class_type::*)(argument_list...);
    if (this->default_storage)
    { 
        // capture allocation's strong pointer
        const mmap::core::allocator<data_type>::pointer_type
        key = handle.lock();

        // search for allocation's metadata by strong pointer as key
        using array = mmap::core::allocator<data_type>::array;
        for (auto &metadata: std::get<array>(this->storage))
        {
            if (metadata.memory_pointer != key)
                continue;

            // auxiliary routine execution
            if constexpr (sys::memory::auxiliary<function_type>)
            {
                // stateful routines need to update metadata
                if constexpr 
                (std::same_as<function_type, decltype(sys::memory::protect)>)
                {
                    metadata.mapping_protocol 
                        |= std::get<2>(std::forward_as_tuple(arguments...));
                }

                else if constexpr
                (std::same_as<function_type, decltype(sys::memory::advise)>)
                {
                    metadata.paging_advice
                        |= std::get<2>(std::forward_as_tuple(arguments...));
                }

                // auxiliary routine execution
                return (std::forward<object_type>(object).*function)
                (
                    metadata.memory_pointer.get(),
                    std::forward<argument_list>(arguments)...
                );
            }

            // aggregate routine execution
            return (std::forward<object_type>(object).*function)
            (
                std::forward<argument_list>(arguments)...
            );
        }
    }

    // table-based storage execution
    else
    {
        // weak pointer is table key
        const auto &key = handle;

        // save table reference
        using table = mmap::core::allocator<data_type>::table;
        table &storage = std::get<table>(this->storage);

        // auxiliary routine execution
        if constexpr (sys::memory::auxiliary<function_type>)
        {
            // stateful routines should update metadata 
            auto metadata = storage.find(key);
            if (metadata != storage.end())
            {
                // stateful routines need to update metadata
                if constexpr 
                (std::same_as<function_type, decltype(sys::memory::protect)>)
                {
                    metadata->mapping_protocol 
                        |= std::get<2>(std::forward_as_tuple(arguments...));
                }

                else if constexpr
                (std::same_as<function_type, decltype(sys::memory::advise)>)
                {
                    metadata->paging_advice
                        |= std::get<2>(std::forward_as_tuple(arguments...));
                }

                // auxiliary routine execution
                return (std::forward<object_type>(object).*function)
                (
                    metadata->memory_pointer.get(),
                    std::forward<argument_list>(arguments)...
                );
            }

            else 
            {
                // throw on no allocation metadata found
                util::log::error<std::runtime_error>
                (
                    "Executor was unable execute function on possible allocation "
                    "becuase no data on such an allocation exists internally",
                    util::log::type::ERROR
                );
            }
        } 

        // aggregate routine execution
        else return (std::forward<object_type>(object).*function)
        (
            std::forward<argument_list>(arguments)...
        );
    }

    // throw on no allocation metadata found
    util::log::error<std::runtime_error>
    (
        "Executor was unable execute function on possible allocation "
        "becuase no data on such an allocation exists internally",
        util::log::type::ERROR
    );
}


/**
 *  \fn Stateful Allocator: Auxiliary and Aggregate Class Const Routine Executor 
 *  
 *  Provides safely ensured execution of other m-family routines part of a class
 *  on an allocation provided it's memory handle.  
 *
 *  Note, all permitted methods are by definition non-modifying on any 
 *  containing class, so this acts as a wrapper.
 *
 *  This does not, however, imply these routines are non-modifying on the 
 *  allocations and their metadata in any manner whatsoever.
 *
 *  \param function:  the function to operate on the allocation
 *  \param object:    the object who this executed function is a member of
 *  \param arguments: all arguments to to-be executed function except address
 *  \param handle:    handle who's allocation is a operable argument
 *
 *  \ret   status:    executed function's return value
 */
template <typename T>
template 
<
    typename    return_type, 
    class       class_type, 
    typename... argument_list, 
    typename    object_type
>
requires sys::memory::auxiliary<return_type (class_type::*)(argument_list...)> 
      || sys::memory::aggregate<return_type (class_type::*)(argument_list...)>
auto mmap::core::allocator<T>::execute
(
    return_type (class_type::*function)(argument_list...) const,
    object_type                              &&object, 
    argument_list                         &&...arguments,
    mmap::core::allocator<T>::handle_type     &handle
) -> decltype
(
    (std::forward<object_type>(object).*function)
    (std::forward<argument_list>(arguments)...)
)
{
    using data_type = T;

    using function_type = return_type (class_type::*)(argument_list...);
    return mmap::core::allocator<data_type>::execute
    (
        reinterpret_cast<function_type>(function),
        const_cast<std::remove_cvref_t<object_type> &>(object),
        std::forward<argument_list>(arguments)...,
        handle
    );
}


/**
 *  \fn Stateful Allocator: In-Place Object Constructor
 *  
 *  Object of type U will be constructed at the address provided with the
 *  appropriate argument list, and validity of memory handle is checked.
 *
 *  \param handle:    handle for the allocation where to construct the object
 *  \param arguments: arguments to objects constructor
 */
template <typename T>
template <typename U, typename... argument_list>
void 
mmap::core::allocator<T>::construct
(
    mmap::core::allocator<T>::handle_type     &handle,
    argument_list                         &&...arguments
) 
{
    using data_type = T;

    // validate memory handle 
    if (handle.expired())
    {
        util::log::error<std::runtime_error>
        (
            "Argument to construction was an invalid weak reference to a"
            "possible allocation",
            util::log::type::FLAG
        );
    } 

    // capture allocation's strong pointer
    const mmap::core::allocator<data_type>::pointer_type
    memory_pointer = handle.lock();

    // construct object at address
    ::new (static_cast<sys::memory::address_type>(memory_pointer.get())) U
    (
        std::forward<argument_list>(arguments)...
    );
}

/**
 *  \fn Stateful Allocator: In-Place Object Destructor
 *  
 *  Object of type U will be destructed from the handle provided.  Validity of 
 *  handle is checked, but interprability is not checked and is a user
 *  resposiblity; exception will be raised on failure.
 *
 *  \param handle: handle for the allocation where to destroy the argument 
 *                 object at 
 */
template <typename T, typename U>
void 
destroy
(
    typename mmap::core::allocator<T>::handle_type &handle
) 
{
    using data_type = T;

    // validate memory handle 
    if (handle.expired())
    {
        util::log::error<std::runtime_error>
        (
            "Argument to destruction was an invalid weak reference to a"
            "possible allocation",
            util::log::type::FLAG
        );
    } 

    // capture allocation's strong pointer
    const typename mmap::core::allocator<data_type>::pointer_type
    memory_pointer = handle.lock();

    // destroy object at address
    if (memory_pointer.get())
        memory_pointer.get()->~U();
}


/**
 *  \fn Stateful Allocator: Max Size / Capacity
 *  
 *  Retrieve the capacity of the a single allocation
 */
template <typename T> 
mmap::core::allocator<T>::size_type
inline mmap::core::allocator<T>::capacity() 
const noexcept
{
    return static_cast<mmap::core::allocator<T>::size_type>
    (
        sys::memory::address_space_limit()
    );
}


/**
 *  \fn Stateful Allocator: Address of References
 *  
 *  Get the address of a given argument object
 *
 *  \param instance: an instance of the object
 */
template <typename T>
mmap::core::allocator<T>::handle_type
mmap::core::allocator<T>::address
(
    T &instance
) const noexcept 
{
    return &instance;
}

template <typename T>
const mmap::core::allocator<T>::handle_type
mmap::core::allocator<T>::address
(
    const T &instance
) const noexcept 
{
    return &instance;
}


/**
 *  \fn Stateful Allocator: Internal Allocator
 *  
 *  Internal allocator for chunk allocation and mapping.  Wraps mmap system 
 *  call to kernel.
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
mmap::core::allocator<T>::handle_type
inline mmap::core::allocator<T>::__allocator__
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
 *  \fn Stateful Allocator: Internal Reallocator
 *  
 *  Internal reallocator for chunk rellocation and remapping.  Wraps mremap
 *  system call to kernel.  
 *
 *  \note Class level access only.
 *
 *  \param memory_address:  memory adddress for already allocated chunk
 *  \param memory_capacity: size of current chunk in bytes
 *  \param newset_capacity: size of new chunk in bytes
 *  \param remapping_flags: indicators for the type of remapping
 */
template <typename T>
mmap::core::allocator<T>::handle_type
inline mmap::core::allocator<T>::__reallocator__
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
 *  \fn Stateful Allocator: Internal Deallocator
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
inline mmap::core::allocator<T>::__deallocator__
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


/**
 *  \fn Allocation Metadata: Copy Constructor and Assignment
 *  
 *  Copies complete state of allocation and memory handle will increase in 
 *  number of references.
 *
 *  \param other: l-value reference to another allocator (copy argument)
 */
template <typename T>
template <typename U>
mmap::core::allocator<T>::metadata_type::metadata_type
(
     const mmap::core::allocator<U>::metadata_type &other
) noexcept:

    // memory data 
    memory_pointer   (other.memory_pointer),
    memory_capacity  (other.memory_capacity),

    // allocation options
    mapping_flags    (other.mapping_flags),
    mapping_protocol (other.mapping_protocol),
    remapping_flags  (other.remapping_flags),
    paging_advice    (other.paging_advice),

    // file data
    file_descriptor  (other.file_descriptor),
    file_offset      (other.file_offset)
{}

template <typename T>
template <typename U> 
mmap::core::allocator<T>::metadata_type &
mmap::core::allocator<T>::metadata_type::operator=
(
    const mmap::core::allocator<U>::metadata_type &other
) noexcept 
{
    if (this == &other)
        return *this;

    // memory data 
    this->memory_pointer   = other.memory_pointer;
    this->memory_capacity  = other.memory_capacity;

    // allocation options
    this->mapping_flags    = other.mapping_flags;
    this->mapping_protocol = other.mapping_protocol;
    this->remapping_flags  = other.remapping_flags;
    this->paging_advice    = other.paging_advice;

    // file data
    this->file_descriptor  = other.file_descriptor;
    this->file_offset      = other.file_offset;

    return *this;
}


/**
 *  \fn Allocation Metadata: Move Constructor and Assignment
 *  
 *  Moves complete state of allocation and invalidates original instance.
 *
 *  \param other: r-value reference to another allocator (move argument)
 */
template <typename T>
template <typename U>
mmap::core::allocator<T>::metadata_type::metadata_type
(
    mmap::core::allocator<U>::metadata_type &&other
) noexcept:

    // memory data 
    memory_pointer   
        (std::exchange(other.memory_pointer,  sys::memory::DEFAULT_BASE)),
    memory_capacity  
        (std::exchange(other.memory_capacity, sys::memory::ZERO_SPACE)),

    // allocation options
    mapping_flags    
        (std::exchange(other.mapping_flags,    sys::memory::NO_FLAG)),
    mapping_protocol 
        (std::exchange(other.mapping_protocol, sys::memory::NO_PROTOCOL)),
    remapping_flags  
        (std::exchange(other.remapping_flags,  sys::memory::NO_FLAG)),
    paging_advice
        (std::exchange(other.paging_advice,    sys::memory::NO_ADVICE)),

    // file data
    file_descriptor  
        (std::exchange(other.file_descriptor, sys::file::INVALID_FILE)),
    file_offset      
        (std::exchange(other.file_offset,     sys::file::ZERO_OFFSET))
{}

template <typename T>
template <typename U>
mmap::core::allocator<T>::metadata_type &
mmap::core::allocator<T>::metadata_type::operator=
(
    mmap::core::allocator<U>::metadata_type &&other
) noexcept
{
    if (this == &other)
        return *this;
    
    // memory data 
    this->memory_pointer   
        = std::exchange(other.memory_pointer,  sys::memory::DEFAULT_BASE);
    this->memory_capacity  
        = std::exchange(other.memory_capacity, sys::memory::ZERO_SPACE);

    // allocation options
    this->mapping_flags    
        = std::exchange(other.mapping_flags,    sys::memory::NO_FLAG);
    this->mapping_protocol 
        = std::exchange(other.mapping_protocol, sys::memory::NO_PROTOCOL);
    this->remapping_flags  
        = std::exchange(other.remapping_flags,  sys::memory::NO_FLAG);
    this->paging_advice
        = std::exchange(other.paging_advice,    sys::memory::NO_ADVICE);

    // file data
    this->file_descriptor  
        = std::exchange(other.file_descriptor, sys::file::INVALID_FILE);
    this->file_offset      
        = std::exchange(other.file_offset,     sys::file::ZERO_OFFSET);

    return *this;
}
