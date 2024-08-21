# Add headers as interface libraries
add_library(mmap_system INTERFACE)
add_library(file_system INTERFACE)

# Specify include directories for the interface library
target_include_directories(mmap_system INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(file_system INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
