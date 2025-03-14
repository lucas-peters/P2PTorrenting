#ifndef BOOTSTRAP_HEARTBEAT_HPP
#define BOOTSTRAP_HEARTBEAT_HPP

#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <libtorrent/socket.hpp>
#include <boost/asio.hpp>
#include "gossip.pb.h"
#include "gossip/gossip.hpp"

// Custom hash function for lt::tcp::endpoint
namespace std {
    template<>
    struct hash<lt::tcp::endpoint> {
        std::size_t operator()(const lt::tcp::endpoint& endpoint) const {
            std::size_t h1 = std::hash<std::string>()(endpoint.address().to_string());
            std::size_t h2 = std::hash<int>()(endpoint.port());
            return h1 ^ (h2 << 1); // Combine the hashes
        }
    };
}

namespace torrent_p2p {

class BootstrapHeartbeat {
public:
    BootstrapHeartbeat(Gossip& gossip, const std::vector<lt::tcp::endpoint>& nodes, 
                      const std::string& ip, const int port);
    virtual ~BootstrapHeartbeat();

    void start();
    void stop();
    void addNode(const lt::tcp::endpoint& node);
    void removeNode(const lt::tcp::endpoint& node);
    bool isNodeAlive(const lt::tcp::endpoint& node) const;
    virtual void processHeartbeatResponse(const lt::tcp::endpoint& sender);

protected:
    const std::string ip_;
    const int port_;
    lt::tcp::endpoint self_endpoint_;
    Gossip& gossip_;
    std::vector<lt::tcp::endpoint> nodes_;
    std::unordered_map<lt::tcp::endpoint, std::chrono::steady_clock::time_point> last_heartbeat_;
    std::unique_ptr<std::thread> heartbeat_thread_;
    std::atomic<bool> running_;
    mutable std::mutex nodes_mutex_;
    const std::chrono::seconds HEARTBEAT_INTERVAL{10};
    const std::chrono::seconds NODE_TIMEOUT{45};

    virtual void heartbeatLoop();
    virtual void sendHeartbeats();
    virtual void checkNodeHealth();
    
    // Helper method to create a heartbeat message with the appropriate type
    //virtual GossipMessage createHeartbeatMessage(HeartbeatMessage::Type type) const;
};

} // namespace torrent_p2p

#endif