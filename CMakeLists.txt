cmake_minimum_required(VERSION 3.10)
project(mmap VERSION 1.0.0)

# C++ options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add out of source tree libraries
add_subdirectory(${CMAKE_SOURCE_DIR}/lib)

# Add source directory
add_subdirectory(${CMAKE_SOURCE_DIR}/src)
