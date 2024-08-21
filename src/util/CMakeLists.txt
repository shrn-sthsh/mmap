# Define local headers & sources
set(LOG_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/record.cpp
)

# Create the library from the source files
add_library(
  log STATIC ${LOG_SOURCES}
)

# Add headers to includes
target_include_directories(
  log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)
