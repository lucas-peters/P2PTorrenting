cmake_minimum_required(VERSION 3.10)
project(P2PTorrenting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libtorrent package
find_package(LibtorrentRasterbar REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread chrono)

# Include libtorrent directories
include_directories(${LibtorrentRasterbar_INCLUDE_DIRS})

# Bootstrap node executable
set(BOOTSTRAP_SOURCES
    src/bootstrap/main.cpp
    src/bootstrap/bootstrap_node.cpp
)
add_executable(p2p_bootstrap ${BOOTSTRAP_SOURCES})
target_link_libraries(p2p_bootstrap PRIVATE LibtorrentRasterbar::torrent-rasterbar Boost::thread Boost::chrono)
target_include_directories(p2p_bootstrap PRIVATE src/bootstrap)

# Client executable
set(CLIENT_SOURCES
    src/client/main.cpp
    src/client/client.cpp
)
add_executable(p2p_client ${CLIENT_SOURCES})
target_link_libraries(p2p_client PRIVATE LibtorrentRasterbar::torrent-rasterbar Boost::thread Boost::chrono)
target_include_directories(p2p_client PRIVATE src/client)

# Test executable
# set(TEST_SOURCES
#     src/client/client_test.cpp
#     src/client/client.cpp
# )
# add_executable(p2p_test ${TEST_SOURCES})
# target_link_libraries(p2p_test PRIVATE LibtorrentRasterbar::torrent-rasterbar)
# target_include_directories(p2p_test PRIVATE src/client)