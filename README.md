# P2PTorrenting
A fully decentralized P2P torrent system based on BitTorrent and Kademlia DHT using libtorrent.

## Features
- Decentralized network using Kademlia DHT
- Bootstrap nodes for network discovery
- Torrent download and upload functionality
- Real-time progress tracking
- DHT statistics monitoring

## Prerequisites
- C++17 compatible compiler
- CMake 3.10 or higher
- libtorrent-rasterbar library

### Installing Dependencies

On macOS:
```bash
brew install libtorrent-rasterbar
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Running a Bootstrap Node
To start a bootstrap node that other peers can use to join the network:

```bash
./p2p_torrent bootstrap [port]
```
Default port is 6881 if not specified.

### Running a Client
To start a client that can download and share torrents:

```bash
./p2p_torrent client [port]
```
Default port is 6882 if not specified.

### Network Setup
1. Start at least one bootstrap node
2. Start multiple clients, they will automatically connect to the bootstrap node
3. Clients can then share and download torrents from each other

## Implementation Details
- Uses Kademlia DHT for peer discovery
- Bootstrap nodes help new peers join the network
- Clients maintain DHT routing tables for efficient peer lookup
- Implements BitTorrent protocol for file sharing

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.

## License
This project is licensed under the MIT License - see the LICENSE file for details.
