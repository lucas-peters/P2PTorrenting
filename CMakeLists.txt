cmake_minimum_required(VERSION 3.10)
project(P2PTorrenting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libtorrent package
find_package(LibtorrentRasterbar REQUIRED)

# Bootstrap node executable
set(BOOTSTRAP_SOURCES
    src/bootstrap/main.cpp
    src/bootstrap/bootstrap_node.cpp
)
add_executable(p2p_bootstrap ${BOOTSTRAP_SOURCES})
target_link_libraries(p2p_bootstrap PRIVATE LibtorrentRasterbar::torrent-rasterbar)
target_include_directories(p2p_bootstrap PRIVATE src/bootstrap)

# Client executable
set(CLIENT_SOURCES
    src/client/main.cpp
    src/client/client.cpp
)
add_executable(p2p_client ${CLIENT_SOURCES})
target_link_libraries(p2p_client PRIVATE LibtorrentRasterbar::torrent-rasterbar)
target_include_directories(p2p_client PRIVATE src/client)
