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


This project uses git submodules for dependencies. After cloning, run:


```bash
git submodule update --init --recursive


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

## Stop and remove all running containers and images:
docker stop $(docker ps -q)
docker rm $(docker ps -aq)
docker rmi $(docker images -q)

## Build Images
docker build -t my-bootstrap-image -f Dockerfile.bootstrap .
docker build -t my-client-image -f Dockerfile.client .
docker build -t my-index-image -f Dockerfile.index .


## Run a bootstrap node //right is the container - left is the host
```
docker run -d \
 --name bootstrap1 \
 --net p2p-network \
 --ip 172.20.0.2 \
 -p 6881:6881/tcp \
 -p 6881:6881/udp \
 -p 7881:7881/tcp \
 -p 7881:7881/udp \
 -e GOSSIP_HOST=172.20.0.2 \
 my-bootstrap-image

```

## Run 2 clients
```
docker run -it \
 --name client1 \
 --net p2p-network \
 --ip 172.20.0.4 \
 -p 6885:6881/tcp \
 -p 6885:6881/udp \
 -p 7885:7881/tcp \
 -p 7885:7881/udp \
 -p 8080:8080/tcp \
 --env GOSSIP_HOST=172.20.0.4 \
 my-client-image

```
```
docker run -it \
 --name client2 \
 --net p2p-network \
 --ip 172.20.0.5 \
 -p 6886:6881/tcp \
 -p 6886:6881/udp \
 -p 7886:7881/tcp \
 -p 7886:7881/udp \
 -p 8081:8080/tcp \
 --env GOSSIP_HOST=172.20.0.5 \
 my-client-image

```

## Run an index
docker run -it \
 --name index-node \
 --net p2p-network \
 --ip 172.20.0.9 \
 -p 6883:6881/tcp \
 -p 6883:6881/udp \
 -p 7883:7881/tcp \
 -p 7883:7881/udp \
 --env GOSSIP_HOST=172.20.0.9 \
 my-index-image


## List docker running
docker container ls


## Login to docker container
docker exec -it client2 /bin/sh
generate pedestrian.pdf
add pedestrian.torrent

docker cp "/Users/shanayaanna/Downloads/pedestrian.pdf" client1:/app/downloads/
docker cp client1:/app/torrents/pedestrian.torrent "/Users/shanayaanna/Downloads/"
docker cp "/Users/shanayaanna/Downloads/pedestrian.torrent" client2:/app/torrents/



## How to start docker engine API

​To enable the Docker Engine API on your Mac's localhost, follow these steps:​

Open Docker Desktop Preferences:

- Click on the Docker icon in the status bar and select "Settings"​
- Navigate to the "Docker Engine" tab.​
- Modify the JSON configuration to include the desired API settings. For example, to enable the API on TCP port 2375:​
- Enter
    ```{
    "hosts": ["unix:///var/run/docker.sock", "tcp://127.0.0.1:2375"]
    }```
    This configuration allows Docker to listen for API requests on both the Unix socket and the specified TCP port.
- Apply and Restart Docker

- run `brew install socat`
- run `socat TCP-LISTEN:2375,reuseaddr,fork UNIX-CONNECT:/var/run/docker.sock &`

- run `npm install -g local-cors-proxy`
- `lcp --proxyUrl http://127.0.0.1:2375` to avoid cors issue

docker config
```
{
  "builder": {
    "gc": {
      "defaultKeepStorage": "20GB",
      "enabled": true
    }
  },
  "experimental": false,
  "hosts": [
    "unix:///var/run/docker.sock",
    "tcp://127.0.0.1:2375"
  ],
"api-cors-header": "*"
}
```


