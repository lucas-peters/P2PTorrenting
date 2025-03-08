#ifndef BOOTSTRAP_NODES_H
#define BOOTSTRAP_NODES_H

#include "node/node.hpp"
#include "gossip/bootstrap_heartbeat.hpp"
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/socket.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <memory>
#include <vector>

namespace torrent_p2p {

class BootstrapNode : public Node {
public:
    BootstrapNode(int port = 6881);
    BootstrapNode(int port, const std::string& state_file);
    BootstrapNode(int port, const std::vector<lt::tcp::endpoint>& bootstrap_nodes);
    BootstrapHeartbeat* getHeartbeatManager();
    ~BootstrapNode();

private:
    std::unique_ptr<std::thread> announceTimer_;
    void start() override;
    void stop() override;
    // Handle DHT alerts
    void handleAlerts() override;
    std::unique_ptr<BootstrapHeartbeat> heartbeat_manager_;
    std::vector<lt::tcp::endpoint> bootstrap_nodes_;
    
    void initializeHeartbeat();
};

} // namespace torrent_p2p

#endif