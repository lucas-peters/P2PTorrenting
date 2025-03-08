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

namespace torrent_p2p {

class BootstrapHeartbeat {
public:
    BootstrapHeartbeat(Gossip& gossip, const std::vector<lt::tcp::endpoint>& bootstrap_nodes);
    ~BootstrapHeartbeat();

    void start();
    void stop();
    void addBootstrapNode(const lt::tcp::endpoint& node);
    void removeBootstrapNode(const lt::tcp::endpoint& node);
    bool isBootstrapNodeAlive(const lt::tcp::endpoint& node) const;

private:
    Gossip& gossip_;
    std::vector<lt::tcp::endpoint> bootstrap_nodes_;
    std::unordered_map<lt::tcp::endpoint, std::chrono::steady_clock::time_point> last_heartbeat_;
    std::unique_ptr<std::thread> heartbeat_thread_;
    std::atomic<bool> running_;
    mutable std::mutex nodes_mutex_;
    const std::chrono::seconds HEARTBEAT_INTERVAL{5};
    const std::chrono::seconds NODE_TIMEOUT{15};

    void heartbeatLoop();
    void sendHeartbeats();
    void processHeartbeatResponse(const lt::tcp::endpoint& sender);
    void checkNodeHealth();
};

} // namespace torrent_p2p

#endif // BOOTSTRAP_HEARTBEAT_HPP