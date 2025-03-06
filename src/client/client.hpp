#ifndef CLIENT_H
#define CLIENT_H

#include "node/node.hpp"

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/address.hpp>
#include <libtorrent/create_torrent.hpp>  // For create_torrent
#include <libtorrent/file_storage.hpp>    // For file_storage
#include <libtorrent/entry.hpp>           // For bencoding
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/version.hpp>         // For version checking

#include <boost/functional/hash.hpp>      // For boost::hash_combine

#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <chrono>

namespace torrent_p2p {

// custom hash function for lt::tcp::endpoint
// had to create this because compiler in docker did not have a way to hash endpoints automatically
struct EndpointHash {
    std::size_t operator()(const lt::tcp::endpoint& ep) const {
        std::size_t seed = 0;
        
        const auto& addr = ep.address();
        if (addr.is_v4()) {
            // IPv4 address
            auto v4addr = addr.to_v4().to_uint();
            boost::hash_combine(seed, v4addr);
        } else {
            // IPv6 address
            auto v6addr = addr.to_v6().to_bytes();
            for (auto b : v6addr) {
                boost::hash_combine(seed, b);
            }
        }
        
        // Combine with port
        boost::hash_combine(seed, ep.port());
        return seed;
    }
};

// This struct is used to track which peers were responsible for contributing data to a piece
// Unfortunately we can't determine exactly which peer sent the bad piece, 
// but we will track all pieces for the torrent, sending messages about what peers contributed to good and bad pieces
// A malicious peer will most likely contribute to mostly bad pieces
// A good piece will increase reputation slightly a bad piece significantly hurts reputation
struct PieceTracker {
    // Track piece contributions - using our custom hash function for endpoints
    std::unordered_map<lt::piece_index_t, 
                      std::unordered_map<lt::tcp::endpoint, 
                                        std::set<int>, 
                                        EndpointHash>> piece_contributors;

    // Track peer reputation changes for this torrent - using our custom hash function
    std::unordered_map<lt::tcp::endpoint, int, EndpointHash> peer_reputation_delta;

    static constexpr int GOOD_PIECE = 1;
    static constexpr int BAD_PIECE = -5;  // Penalize bad pieces more heavily

    void record_bad_piece(lt::piece_index_t piece) {
        if (piece_contributors.find(piece) == piece_contributors.end())
            return;

        for(auto& [peer, block] : piece_contributors[piece]) {
            peer_reputation_delta[peer] += BAD_PIECE;
        }
        piece_contributors.erase(piece);
    }

    void record_good_piece(lt::piece_index_t piece) {
        if (piece_contributors.find(piece) == piece_contributors.end())
            return;
        for(auto& [peer, block] : piece_contributors[piece]) {
            peer_reputation_delta[peer] += GOOD_PIECE;
        }
        piece_contributors.erase(piece);
    }

    std::vector<lt::tcp::endpoint> get_piece_contributors(lt::piece_index_t piece) {
        std::vector<lt::tcp::endpoint> peer_return;
        for (auto& peer : piece_contributors[piece]) {
            peer_return.push_back(peer.first);
        }
        return peer_return;
    }

    void add_block(lt::piece_index_t piece, int block, lt::tcp::endpoint const& peer) {
        piece_contributors[piece][peer].insert(block);
    }

    std::vector<std::pair<lt::tcp::endpoint, int>> get_reputation_updates() {
        std::vector<std::pair<lt::tcp::endpoint, int>> updates;
        for (auto& [peer, delta] : peer_reputation_delta) {
            updates.emplace_back(peer, delta);
        }
        return updates;
    }
};

class Client : public Node {
public:
    Client(int port = 6881);
    Client(int port, const std::string& state_file);
    ~Client();

    // bootstraps into the dht using set endpoints at bootstrap nodes
    void connectToDHT(const std::vector<std::pair<std::string, int>>& bootstrap_nodes);
    
    // Add a torrent to download
    void addTorrent(const std::string& torrentFilePath, const std::string& savePath);
    bool hasTorrent(const lt::sha1_hash& hash) const;

    // turns a normal file into a torrent file
    void generateTorrentFile(const std::string& savePath);
    
    // Get progress of a specific torrent
   //double getProgress(const std::string& torrentFilePath);

    // Print the status of all torrents
    void printStatus() const;

    // Seach DHT network for peers with a specific info hash
    void searchDHT(const std::string& infoHash);

    // Generate a magnet URI for a specific torrent
    std::string createMagnetURI(const std::string& torrentFilePath) const;

    // Add a magnet link to the client
    void addMagnet(const std::string& infoHash, const std::string& savePath);

    // by default a torrent starts seeding when added, these can be used to turn those flags on/off
    void startSeeding(const std::string& torrentFilePath);
    void stopSeeding(const std::string& torrentFilePath);

    // Converts a string representation of an info hash to a sha1_hash
    lt::sha1_hash stringToHash(const std::string& infoHashString) const;

    // save/load the client's DHT state to/from a file
    bool saveDHTState(const std::string& state_file) const;
    bool loadDHTState(const std::string& state_file);

private:
    // tracks what peers contributed what pieces of each torrent
    std::unordered_map<lt::sha1_hash, std::unique_ptr<PieceTracker>> torrent_trackers_;
    std::mutex torrent_tracker_mutex_;

    std::mutex peers_mutex_;
    std::unordered_map<lt::tcp::endpoint, int, EndpointHash> peer_cache_; // maps port to reputation, currently not tracking when peer was last seen
    
    void updatePeerCache();
    void addPeerToCache(const std::string& ip, int port, const std::string& id = "");
    void updatePeerReputation(const std::string& peer_key, int delta);
    
    //std::vector<PeerInfo> getRandomPeers(size_t count);
    std::vector<std::pair<std::string, int>> bootstrap_nodes_ = {{"172.20.0.2", 6881}};
    std::map<lt::sha1_hash, lt::torrent_handle> torrents_;

    // Start/stop the client
    void start() override;
    void stop() override;

    // Initialize the session and set flags, called by start
    void initializeSession();

    // libtorrent sets alerts to communicate when events happen, such as when a torrent is downloading or when a peer sends a message
    void handleAlerts() override;

    // gossips to the dht network about the reputation of a node
    void sendGossip(std::vector<std::pair<lt::tcp::endpoint, int>> peer_reputation) const;
    
    // callback for incoming gossip message
    void handleReputationMessage(const ReputationMessage& message, const lt::tcp::endpoint& sender);

    //threads
    std::unique_ptr<std::thread> alert_thread_;
    std::unique_ptr<std::thread> peer_cache_thread_;

    // this is where we store torrents and downloads on docker images
    std::string torrent_path_ = "/app/torrents/";
    std::string download_path_ = "/app/downloads/";
    
    // Ban a node from the DHT network
    void banNode(const lt::tcp::endpoint& endpoint);
};

} // namespace torrent_p2p

#endif