# P2PTorrenting
A fully decentralized P2P torrent system based on BitTorrent and Kademlia DHT using libtorrent-rasterbar.

## Project Overview
This project implements a complete peer-to-peer torrenting system with the following components:
- **Bootstrap Nodes**: Entry points to the network that help peers discover each other
- **Index Nodes**: Maintain a searchable index of available torrents
- **Client Nodes**: End-user applications that download and share torrents

### How to Run Locally
- Install docker and make sure the docker daemon thread is running
- The easiest way to run the project locally is to use 'docker compose up --build -d'
- By default I have this set to create 5 bootstraps, 5 indexes and 5 clients
- You can use 'docker attach client[1-5]' to attach to stdin of the clients and use the CLI

### Running on AWS
- In order to run on AWS I have a script that automatically checks for the latest docker images pushed to my registry
- I then loop through and ssh into each node and call 'docker-compose up -d'
- The script can only work with my access key, so please reach out to me if you want to run the full system
- lmpeters@scu.edu or (408) 429-5071

### Important Classes

#### Node Library (`src/node/`)
- Base class for all nodes
- Handles initialization, settings, flags, and creating Gossip and Messenger objects
- DHT initialization and management
- Network configuration
- Implements the core BitTorrent protocol

#### Gossip System (`src/gossip/`)
- Peer-to-peer messaging protocol using Protocol Buffers
- Used to implement our heartbeating system
- Reputation message propagation
- Lamport clocks for message ordering, messages are placed into a priority queue when they are received

#### Bootstrap Node (`src/bootstrap/`)
- Network entry point
- Maintains routing tables for peer discovery
- Helps new peers join the network
- Monitors node health via heartbeats

#### Index Node (`src/index/`)
- Maintains searchable torrent database
- Handles search queries from clients
- Stores torrent metadata (title, magnet links)
- Uses Messenger class for communication with clients

#### Client (`src/client/`)
- End-user application
- Downloads and shares torrents
- Tracks peer reputation based on piece contribution quality
- Monitors download progress and speed
- Implements piece verification and corruption detectio

### Client Commands (CLI)
Once a client is running, you can use the following commands:
- `add <torrent_file>`: Add a torrent file for downloading/seeding
- `magnet <magnet_uri>`: Add a torrent using a magnet link
- `search <keyword>`: Search for torrents in the index, returns a magnet link
- `status`: Show download status of all torrents
- `dht`: Show DHT statistics
- `corrupt <info_hash> <percentage>`: Corrupt a percentage of a torrent (for testing reputation system)
- `quit`: Exit the client

## Implementation Details

### DHT Network
The system uses Kademlia DHT for peer discovery, with each node maintaining a routing table of known peers. Bootstrap nodes serve as entry points to help new peers join the network. The DHT implementation is based on libtorrent's implementation with custom extensions.

### Heartbeat System
Nodes send periodic heartbeat messages to monitor the health of other nodes in the network:
- Bootstraps and Indexes periodically send out pings through the gossiping protocol
- When one of these nodes receives a ping, it sends a pong back through gossip
- A node is locally marked as alive every time a pong is received
- If a node is not marked as alive after a timeout, it is considered dead

### Reputation System
Clients track the reputation of peers based on their behavior:
- Unfortunately this didn't fully work because libtorrent was detecting the bad pieces before our system could
- PieceTracker monitors which peers contributed to each downloaded piece
- Good pieces increase peer reputation slightly (+1)
- Bad pieces significantly decrease reputation (-5)
- Only neighboring nodes track reputation scores of their peers
- When a node detects a good or bad piece it gossips to the entire network
- The idea is that if all neighboring nodes blacklist a peer, he will be choked out of the network
- Reputation information is gossiped through the network
- Peers start with a reputation score of 100, and if they fall below zero they are banned

### Torrent Indexing
Index nodes maintain a searchable database of torrents:
- Keyword-based search
- Titles are split into keywords
- Keywords are mapped to titles and titles are mapped to magnet links
- Magnet links allow any client to start downloading without needing the .torrent file
- Keywords are distributed across nodes using sharding
- The keyword is hashed and then I take the modulo of the hash to determine what index id to store it at
- The keyword is also stored at the next two neighboring nodes for replication and fault tolerance
- When a user sends a query to an index it first checks whether it is responsible,
    if not, it sends the query to the responsible

## License
This project is licensed under the MIT License - see the LICENSE file for details.
