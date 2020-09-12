cmake_minimum_required(VERSION 2.8)

project(RCO_Monitor)

set(DEPENDENCY_DIR ${CMAKE_SOURCE_DIR}/deps)
set(SPDLOG_DIR ${DEPENDENCY_DIR}/spdlog)
set(SPDLOG_INCLUDE_DIR ${SPDLOG_DIR}/include)

# Executable name
set(TARGET_NAME "RCO_Monitor")

# Set the source directory
set(LIGHTCTRL_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Set the include directory
set(LIGHTCTRL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# Set the src files for the project
file(GLOB SRC_FILES "${LIGHTCTRL_SRC_DIR}/*.c*")

# Set project includes dir
include_directories(
  "${LIGHTCTRL_INCLUDE_DIR}"
  "${SPDLOG_INCLUDE_DIR}"
  )

add_subdirectory(${SPDLOG_DIR})

# Adding executable
add_executable(${TARGET_NAME} ${SRC_FILES})
set_target_properties(${TARGET_NAME} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(${TARGET_NAME}
  spdlog
  pthread
  )

add_custom_target(move_compile_commands ALL
  COMMAND cmake -E create_symlink ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json
  DEPENDS ${TARGET_NAME}
  )
