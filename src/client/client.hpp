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
    Client(int port = 6881, const std::string& env = "aws", const std::string& ip = "None");
    Client(int port, const std::string& env, const std::string& ip, const std::string& state_file);
    ~Client();
    
    // methods that support adding torrents to libtorrent session
    void addTorrent(const std::string& torrentFilePath);
    void addMagnet(const std::string& magnet);
    bool hasTorrent(const lt::sha1_hash& hash) const;
    void generateTorrentFile(const std::string& fileName);
    std::string createMagnetURI(const std::string& torrentFilePath) const;
    lt::sha1_hash stringToHash(const std::string& infoHashString) const; // converts a string representation of an info hash to a sha1_hash
    
    void pauseTorrent(const std::string& name);
    void resumeTorrent(const std::string& name);
    void removeTorrent(const std::string& name);

    lt::torrent_handle getTorrentHandleByName(const std::string& name) const; // Get handle by name
    
    void searchDHT(const std::string& infoHash); // Seach DHT network for peers with a specific info hash, not really necessary

    void addIndex(const std::string& title, const std::string& magnet);
    void searchIndex(const std::string& keyword);

    void corruptTorrentData(const lt::sha1_hash& info_hash, double corruption_percent = 10.0);
    void corruptAllTorrents(double corruption_percentage);

    // logging download/upload stats
    void printStatus() const;
    void startDownloadTimer(const lt::sha1_hash& info_hash);
    void reportDownloadCompletion(const lt::sha1_hash& info_hash);
    std::string formatSize(std::int64_t bytes) const;

private:
    // tracks all torrents currently in our session
    std::map<lt::sha1_hash, lt::torrent_handle> torrents_; 
    mutable std::mutex torrents_mutex_;

    void start() override;
    void stop() override;

    // libtorrent sets alerts to communicate when events happen, such as when a torrent is downloading or when a peer sends a message
    void handleAlerts() override;

    // automatically sets save paths to correct file path inside of our docker containers
    void setSavePaths(const std::string& env);

    // tracks what peers contributed what pieces of each torrent for reputation system
    std::unordered_map<lt::sha1_hash, std::unique_ptr<PieceTracker>> torrent_trackers_;
    mutable std::mutex torrent_tracker_mutex_;

    // peer cache for reputation system
    std::unordered_map<lt::tcp::endpoint, int, EndpointHash> peer_cache_;
    std::mutex peers_mutex_;
    void updatePeerCache();
    void addPeerToCache(const std::string& ip, int port, const std::string& id = "");
    void updatePeerReputation(const std::string& peer_key, int delta);
    // when reputation reaches a low enoug threshold, ban the node
    void banNode(const lt::tcp::endpoint& endpoint); 
    // gossips to the dht network about the reputation of a node
    void sendGossip(std::vector<std::pair<lt::tcp::endpoint, int>> peer_reputation) const;
    void handleReputationMessage(const ReputationMessage& message, const lt::tcp::endpoint& sender);

    void handleIndexMessage(const IndexMessage& message);

    //threads
    std::unique_ptr<std::thread> alert_thread_;
    std::unique_ptr<std::thread> peer_cache_thread_;

    // will be overwritten if we set env flag to docker or aws
    std::string torrent_path_ = "/Users/lucaspeters/Documents/GitHub/P2PTorrenting/6882/";
    std::string download_path_ = "/Users/lucaspeters/Documents/GitHub/P2PTorrenting/6882/";  

    struct DownloadProgress {
        double last_progress = 0.0;
        double current_progress = 0.0;
        std::chrono::steady_clock::time_point last_update_time;
        double avg_speed_kBs = 0.0;
        double est_time_remaining_sec = 0.0;
    };
    std::map<lt::sha1_hash, DownloadProgress> download_progress_;
    std::map<lt::sha1_hash, std::chrono::steady_clock::time_point> download_start_times_;
    std::mutex progress_mutex_;
};

} // namespace torrent_p2p

#endif