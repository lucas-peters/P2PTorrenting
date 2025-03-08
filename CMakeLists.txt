cmake_minimum_required(VERSION 3.10)
project(P2PTorrenting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# Add Qt for GUI
find_package(Qt5 COMPONENTS Widgets Core Gui REQUIRED)

# Print debug info
message(STATUS "LibtorrentRasterbar include dirs: ${LibtorrentRasterbar_INCLUDE_DIRS}")

include_directories(
    ${LibtorrentRasterbar_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(PROTO_FILES src/gossip/gossip.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Add lamport clock library
add_library(lamport STATIC
    src/gossip/lamport.cpp
)

add_library(gossip STATIC
    src/gossip/gossip.cpp
    ${PROTO_SRCS}
)

target_link_libraries(gossip PRIVATE
    lamport
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
    absl::log
    absl::base
    absl::strings
    absl::raw_logging_internal
    # Add missing Abseil logging libraries
    absl::log_internal_check_op
    absl::log_internal_message
    absl::log_internal_conditions
    absl::log_internal_format
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
    lamport
)

target_include_directories(node PUBLIC 
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

# Add GUI client
add_executable(p2p_gui_client
    src/client/gui_main.cpp
    src/client/client.cpp
    src/client/mainwindow.cpp
    src/client/torrentlistmodel.cpp
)

# Tell CMake to run moc when necessary
set_target_properties(p2p_gui_client PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_link_libraries(p2p_gui_client PRIVATE
    node
    LibtorrentRasterbar::torrent-rasterbar
    Boost::thread
    Boost::chrono
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

target_include_directories(p2p_gui_client PRIVATE src/client PUBLIC src)