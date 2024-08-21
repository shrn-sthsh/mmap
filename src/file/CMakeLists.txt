# Define local headers & sources
set(FILE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/file.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ordered_file.cpp
  # ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_file.cpp
)

# Create the library from the source files
add_library(
  file STATIC ${FILE_SOURCES}
)

# Link dependencies 
target_link_libraries(
    file PRIVATE mmap_system file_system
)

# Add headers to includes
target_include_directories(
  file PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)
