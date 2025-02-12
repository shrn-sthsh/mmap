# Define local headers & sources
set(FILE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/standard_allocator.cpp
)

# Create the library from the source files
add_library(
  core STATIC ${FILE_SOURCES}
)

# Include and link dependencies 
target_link_libraries(
  core PRIVATE type_utility mmap_system file_system 
)

# Add headers to includes
target_include_directories(
  core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)
