cmake_minimum_required(VERSION 3.10)
project(P2PTorrenting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libtorrent package
find_package(LibtorrentRasterbar REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread chrono)

# Include libtorrent directories
include_directories(${LibtorrentRasterbar_INCLUDE_DIRS})

<<<<<<< Updated upstream
=======
# Common include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Node base class library
set(NODE_SOURCES
    src/node/node.cpp
)
add_library(node STATIC ${NODE_SOURCES})
target_link_libraries(node PRIVATE LibtorrentRasterbar::torrent-rasterbar Boost::thread Boost::chrono)
target_include_directories(node PUBLIC src)

# Gossip protocol library
set(GOSSIP_SOURCES
    src/Gossip/Gossip.cpp
)
add_library(gossip STATIC ${GOSSIP_SOURCES})
target_link_libraries(gossip PRIVATE LibtorrentRasterbar::torrent-rasterbar Boost::thread Boost::chrono)
target_include_directories(gossip PUBLIC src)

>>>>>>> Stashed changes
# Bootstrap node executable
set(BOOTSTRAP_SOURCES
    src/bootstrap/main.cpp
    src/bootstrap/bootstrap_node.cpp
)
add_executable(p2p_bootstrap ${BOOTSTRAP_SOURCES})
<<<<<<< Updated upstream
target_link_libraries(p2p_bootstrap PRIVATE LibtorrentRasterbar::torrent-rasterbar Boost::thread Boost::chrono)
target_include_directories(p2p_bootstrap PRIVATE src/bootstrap)
=======
target_link_libraries(p2p_bootstrap PRIVATE node gossip LibtorrentRasterbar::torrent-rasterbar Boost::thread Boost::chrono)
target_include_directories(p2p_bootstrap PRIVATE src/bootstrap PUBLIC src)
>>>>>>> Stashed changes

# Client executable
set(CLIENT_SOURCES
    src/client/main.cpp
    src/client/client.cpp
)
add_executable(p2p_client ${CLIENT_SOURCES})
<<<<<<< Updated upstream
target_link_libraries(p2p_client PRIVATE LibtorrentRasterbar::torrent-rasterbar Boost::thread Boost::chrono)
target_include_directories(p2p_client PRIVATE src/client)
=======
target_link_libraries(p2p_client PRIVATE node gossip LibtorrentRasterbar::torrent-rasterbar Boost::thread Boost::chrono)
target_include_directories(p2p_client PRIVATE src/client PUBLIC src)
>>>>>>> Stashed changes

# Test executable
# set(TEST_SOURCES
#     src/client/client_test.cpp
#     src/client/client.cpp
# )
# add_executable(p2p_test ${TEST_SOURCES})
<<<<<<< Updated upstream
# target_link_libraries(p2p_test PRIVATE LibtorrentRasterbar::torrent-rasterbar)
# target_include_directories(p2p_test PRIVATE src/client)
=======
# target_link_libraries(p2p_test PRIVATE node gossip LibtorrentRasterbar::torrent-rasterbar)
# target_include_directories(p2p_test PRIVATE src/client PUBLIC src)
>>>>>>> Stashed changes
