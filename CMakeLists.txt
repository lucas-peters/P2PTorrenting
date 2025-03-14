cmake_minimum_required(VERSION 3.10)
project(P2PTorrenting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Used for nodes to find config file
set(PROJECT_ROOT_DIR "${CMAKE_SOURCE_DIR}")
add_definitions(-DPROJECT_ROOT="${PROJECT_ROOT_DIR}")

# Use Abseil from submodule
message(STATUS "Using Abseil from submodule")
# Prevent Abseil from overriding our compiler flags
set(ABSL_PROPAGATE_CXX_STD ON)
# Disable Abseil tests to speed up the build
set(BUILD_TESTING OFF CACHE BOOL "Disable Abseil tests")
# Add Abseil from the submodule
add_subdirectory(third_party/abseil-cpp)

# Find required packages
find_package(LibtorrentRasterbar REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread chrono)
find_package(Protobuf REQUIRED)

# Print debug info
message(STATUS "LibtorrentRasterbar include dirs: ${LibtorrentRasterbar_INCLUDE_DIRS}")

include_directories(
    ${LibtorrentRasterbar_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set(PROTO_FILES 
    src/gossip/gossip.proto
    src/index/index.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Define common Abseil dependencies to avoid repetition, protobufer complains if these aren't all here
set(ABSL_DEPS
    absl::log
    absl::base
    absl::strings
    absl::raw_logging_internal
    absl::log_internal_check_op
    absl::log_internal_message
    absl::log_internal_conditions
    absl::log_internal_format
    absl::log_internal_log_sink_set
    absl::log_globals
    absl::log_initialize
    absl::log_severity
    absl::log_entry
    absl::log_sink
    absl::log_sink_registry
    absl::check
    absl::log_internal_nullguard
    absl::log_internal_proto
    absl::log_internal_nullstream
    absl::log_internal_globals
    absl::log_internal_append_truncated
    absl::log_internal_strip
    absl::log_internal_voidify
)

# Add lamport clock library
add_library(lamport STATIC
    src/gossip/lamport.cpp
)

add_library(gossip STATIC
    src/gossip/gossip.cpp
    src/gossip/bootstrap_heartbeat.cpp
    src/gossip/index_heartbeat.cpp
    ${PROTO_SRCS}
)

target_link_libraries(gossip PRIVATE
    lamport
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
    ${ABSL_DEPS}
    protobuf::libprotobuf
)

target_include_directories(gossip PUBLIC
    src
    ${PROTOBUF_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Add messenger library
add_library(messenger STATIC
    src/index/messenger.cpp
)

target_link_libraries(messenger PUBLIC
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
    gossip
    lamport
    protobuf::libprotobuf
    ${ABSL_DEPS}
)

target_include_directories(messenger PUBLIC
    src
    ${PROTOBUF_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# Add node library
add_library(node STATIC src/node/node.cpp)
target_link_libraries(node PUBLIC
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
    gossip
    lamport
    messenger
)

target_include_directories(node PUBLIC 
    src
    ${PROTOBUF_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# Add index library
add_library(index STATIC
    src/index/index.cpp
)

target_link_libraries(index PUBLIC
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
    gossip
    protobuf::libprotobuf
    ${ABSL_DEPS}
    node
    messenger
)

target_include_directories(index PUBLIC
    src
    ${PROTOBUF_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

add_executable(p2p_bootstrap
    src/bootstrap/main.cpp
    src/bootstrap/bootstrap_node.cpp
)

target_link_libraries(p2p_bootstrap PRIVATE
    node
    protobuf::libprotobuf
)

target_include_directories(p2p_bootstrap PRIVATE src/bootstrap PUBLIC src)

add_executable(p2p_client
    src/client/main.cpp
    src/client/client.cpp
)

target_link_libraries(p2p_client PRIVATE
    node
    protobuf::libprotobuf
)

target_include_directories(p2p_client PRIVATE src/client PUBLIC src)

# Add index node executable
add_executable(p2p_index
    src/index/index_main.cpp
)

target_link_libraries(p2p_index PRIVATE
    index
    protobuf::libprotobuf
)

target_include_directories(p2p_index PRIVATE src/index PUBLIC src)
