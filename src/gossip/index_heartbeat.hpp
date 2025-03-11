#ifndef TORRENT_P2P_INDEX_HEARTBEAT_HPP
#define TORRENT_P2P_INDEX_HEARTBEAT_HPP

#include "gossip/gossip.hpp"
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <atomic>

namespace torrent_p2p {

class IndexHeartbeat {
public:
    // Constants for heartbeat timing
    static constexpr std::chrono::seconds HEARTBEAT_INTERVAL{5};  // Send heartbeat every 30 seconds
    static constexpr std::chrono::seconds NODE_TIMEOUT{15};        // Consider node dead after 90 seconds

    // Constructor and destructor
    IndexHeartbeat(Gossip& gossip, const std::vector<lt::tcp::endpoint>& index_nodes);
    ~IndexHeartbeat();

    // Start and stop the heartbeat mechanism
    void start();
    void stop();

    // Manage index nodes
    void addIndexNode(const lt::tcp::endpoint& node);
    void removeIndexNode(const lt::tcp::endpoint& node);
    bool isIndexNodeAlive(const lt::tcp::endpoint& node) const;
    
    // Get all known index nodes
    std::vector<lt::tcp::endpoint> getAliveIndexNodes() const;
    
    // Process heartbeat responses
    void processHeartbeatResponse(const lt::tcp::endpoint& sender);
    
    // Request index synchronization with other nodes
    void requestIndexSync(const lt::tcp::endpoint& target);

private:
    // Main heartbeat loop
    void heartbeatLoop();
    
    // Send heartbeats to all index nodes
    void sendHeartbeats();
    
    // Check health of nodes
    void checkNodeHealth();
    
    // References and state
    Gossip& gossip_;
    std::vector<lt::tcp::endpoint> index_nodes_;
    std::unordered_map<lt::tcp::endpoint, std::chrono::steady_clock::time_point> last_heartbeat_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> heartbeat_thread_;
    mutable std::mutex nodes_mutex_;
};

} // namespace torrent_p2p

#endif