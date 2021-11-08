cmake_minimum_required(VERSION 3.10)

set(CURRENT_VERSION_MAJOR 2)
set(CURRENT_VERSION_MINOR 0)
set(CURRENT_VERSION_PATCH 0)

project(Radio_Monitor VERSION ${CURRENT_VERSION_MAJOR}.${CURRENT_VERSION_MINOR}.${CURRENT_VERSION_PATCH})

set(DEPENDENCY_DIR ${CMAKE_SOURCE_DIR}/deps)
set(SPDLOG_DIR ${DEPENDENCY_DIR}/spdlog)
set(SPDLOG_INCLUDE_DIR ${SPDLOG_DIR}/include)

# Executable name
set(TARGET_NAME "Radio_Monitor-v${CURRENT_VERSION_MAJOR}.${CURRENT_VERSION_MINOR}.${CURRENT_VERSION_PATCH}.rpi")

# Set the source directory
set(LIGHTCTRL_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Firmware/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Firmware/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Firmware/bin)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-rtti")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
add_definitions(-DCURRENT_VERSION_MAJOR=${CURRENT_VERSION_MAJOR})
add_definitions(-DCURRENT_VERSION_MINOR=${CURRENT_VERSION_MINOR})
add_definitions(-DCURRENT_VERSION_PATCH=${CURRENT_VERSION_PATCH})

# Use install RPATH when building
SET(CMAKE_BUILD_WITH_INSTALL_RPATH true)

# the RPATH to be used when installing
SET(CMAKE_INSTALL_RPATH "/home/ubuntu/lib/spdlog")

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

target_link_libraries(${TARGET_NAME}
  pthread
  stdc++fs
  )

if (${FACILITY_TYPE} STREQUAL RTR)
file (COPY ${CMAKE_SOURCE_DIR}/cfg/ANCE_config.json 
DESTINATION ${CMAKE_BINARY_DIR}/Firmware/bin
FILE_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_WRITE GROUP_READ WORLD_READ)
file (RENAME ${CMAKE_BINARY_DIR}/Firmware/bin/ANCE_config.json ${CMAKE_BINARY_DIR}/Firmware/bin/config.json)
endif()


file (COPY ${CMAKE_SOURCE_DIR}/cfg/RCO_config.json
DESTINATION ${CMAKE_BINARY_DIR}/Firmware
FILE_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_WRITE GROUP_READ WORLD_READ)
file (RENAME ${CMAKE_BINARY_DIR}/Firmware/RCO_config.json ${CMAKE_BINARY_DIR}/Firmware/config.json)

file(WRITE ${CMAKE_BINARY_DIR}/tmp/enable_radio_monitor.sh
"chmod +x ${TARGET_NAME} && ln -s ${TARGET_NAME} ${PROJECT_NAME}
sudo systemctl link /home/ubuntu/${PROJECT_NAME}.service
sudo systemctl enable ${PROJECT_NAME}
sudo reboot")

file (COPY ${CMAKE_BINARY_DIR}/tmp/enable_radio_monitor.sh 
DESTINATION ${CMAKE_BINARY_DIR}/Firmware
FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)

file(WRITE ${CMAKE_BINARY_DIR}/Firmware/${PROJECT_NAME}.service
"[Unit]
Description=${PROJECT_NAME} service
StartLimitIntervalSec=0

[Service]
Type=simple
Restart=on-failure
RestartSec=1
User=ubuntu
ExecStart=/home/ubuntu/bin/${PROJECT_NAME}
WorkingDirectory=/home/ubuntu/bin

[Install]
WantedBy=multi-user.target")