#ifndef TORRENT_P2P_INDEX_HEARTBEAT_HPP
#define TORRENT_P2P_INDEX_HEARTBEAT_HPP

#include "gossip/gossip.hpp"
#include "gossip/bootstrap_heartbeat.hpp"
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <libtorrent/socket.hpp>

namespace torrent_p2p {

class IndexHeartbeat : public BootstrapHeartbeat{
public:
    IndexHeartbeat(Gossip& gossip, const std::vector<lt::tcp::endpoint>& index_nodes,
    const std::string& ip, const int port);
    ~IndexHeartbeat();

    // void addIndexNode(const lt::tcp::endpoint& node);
    // void removeIndexNode(const lt::tcp::endpoint& node);
    // bool isIndexNodeAlive(const lt::tcp::endpoint& node) const;
    
    std::vector<lt::tcp::endpoint> getAliveIndexNodes() const;
    void processHeartbeatResponse(const lt::tcp::endpoint& sender) override;
    
    // Request index synchronization with other nodes
    void requestIndexSync(const lt::tcp::endpoint& target);

private:
    void sendHeartbeats() override;
};

} // namespace torrent_p2p

#endif