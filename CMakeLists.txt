cmake_minimum_required(VERSION 3.10)
project(P2PTorrenting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libtorrent package
find_package(LibtorrentRasterbar REQUIRED)

# Add source files
set(SOURCES
    src/bootstrap_node.cpp
    src/client.cpp
    src/main.cpp
)

# Create executable
add_executable(p2p_torrent ${SOURCES})

# Link libraries
target_link_libraries(p2p_torrent PRIVATE LibtorrentRasterbar::torrent-rasterbar)
