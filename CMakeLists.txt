cmake_minimum_required(VERSION 3.10)
project(P2PTorrenting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the path to the locally built Abseil
set(ABSEIL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/abseil-cpp/build)
list(APPEND CMAKE_PREFIX_PATH ${ABSEIL_DIR})

# Find required packages
find_package(LibtorrentRasterbar REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread chrono)
find_package(Protobuf REQUIRED)
find_package(absl REQUIRED CONFIG) # Use CONFIG mode to find the local build

# Print debug info
message(STATUS "Found Abseil at: ${ABSEIL_DIR}")
message(STATUS "LibtorrentRasterbar include dirs: ${LibtorrentRasterbar_INCLUDE_DIRS}")

include_directories(
    ${LibtorrentRasterbar_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(PROTO_FILES src/Gossip/gossip.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(gossip STATIC
    src/Gossip/Gossip.cpp
    ${PROTO_SRCS}
)

target_link_libraries(gossip PRIVATE
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
    absl::log
    absl::base
    absl::strings
    absl::raw_logging_internal
    # Add missing Abseil logging libraries, cmake could not find these I had to declare this directy
    absl::log_internal_check_op
    absl::log_internal_message
    absl::log_internal_conditions
    absl::log_internal_format
    absl::log_internal_globals
    absl::log_internal_log_sink_set
    absl::log_globals
    absl::log_initialize
    protobuf::libprotobuf
)

target_include_directories(gossip PUBLIC
    src
    ${PROTOBUF_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

add_library(node STATIC src/node/node.cpp)
target_link_libraries(node PRIVATE
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
    gossip
)
target_include_directories(node PUBLIC src)

add_executable(p2p_bootstrap
    src/bootstrap/main.cpp
    src/bootstrap/bootstrap_node.cpp
)
target_link_libraries(p2p_bootstrap PRIVATE
    node
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
)
target_include_directories(p2p_bootstrap PRIVATE src/bootstrap PUBLIC src)

add_executable(p2p_client
    src/client/main.cpp
    src/client/client.cpp
)
target_link_libraries(p2p_client PRIVATE
    node
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
)
target_include_directories(p2p_client PRIVATE src/client PUBLIC src)