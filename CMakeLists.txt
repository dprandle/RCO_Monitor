cmake_minimum_required(VERSION 3.10)

set(CURRENT_VERSION_MAJOR 1)
set(CURRENT_VERSION_MINOR 0)
set(CURRENT_VERSION_PATCH 0)

project(RCO_Monitor VERSION ${CURRENT_VERSION_MAJOR}.${CURRENT_VERSION_MINOR}.${CURRENT_VERSION_PATCH})

set(DEPENDENCY_DIR ${CMAKE_SOURCE_DIR}/deps)
set(SPDLOG_DIR ${DEPENDENCY_DIR}/spdlog)
set(SPDLOG_INCLUDE_DIR ${SPDLOG_DIR}/include)

# Executable name
set(TARGET_NAME "RCO_Monitor-v${CURRENT_VERSION_MAJOR}.${CURRENT_VERSION_MINOR}.${CURRENT_VERSION_PATCH}.rpi")

# Set the source directory
set(LIGHTCTRL_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-rtti -fno-exceptions")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
add_definitions(-DCURRENT_VERSION_MAJOR=${CURRENT_VERSION_MAJOR})
add_definitions(-DCURRENT_VERSION_MINOR=${CURRENT_VERSION_MINOR})
add_definitions(-DCURRENT_VERSION_PATCH=${CURRENT_VERSION_PATCH})

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH true)

# the RPATH to be used when installing
SET(CMAKE_INSTALL_RPATH "/home/ubuntu/bin/spdlog")

# Set the src files for the project
file(GLOB SRC_FILES "${LIGHTCTRL_SRC_DIR}/*.c*")

# Set project includes dir
include_directories(
  "${SPDLOG_INCLUDE_DIR}"
  )

add_subdirectory(${SPDLOG_DIR})

if (${CMAKE_BUILD_TYPE} STREQUAL Debug)
  add_definitions(-DDEBUG_VERSION)
else()
  add_definitions(-DRELEASE_VERSION)
endif()

# Adding executable
add_executable(${TARGET_NAME} ${SRC_FILES})
set_target_properties(${TARGET_NAME} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

target_link_libraries(${TARGET_NAME}
  spdlog
  pthread
  stdc++fs
  )

if (${CMAKE_BUILD_TYPE} STREQUAL Release)
add_custom_command(
    TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_BINARY_DIR}/bin/${TARGET_NAME}
    /home/ubuntu/bin/${TARGET_NAME})

add_custom_command(
    TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    /home/ubuntu/bin/${TARGET_NAME}
    /home/ubuntu/bin/RCO_Monitor
    )
endif()
