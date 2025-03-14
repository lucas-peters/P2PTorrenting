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
    BootstrapNode(int port = 6881, const std::string& env = "aws", const std::string& ip = "None");
    BootstrapNode(int port, const std::string& env, const std::string& ip, const std::string& state_file);

    ~BootstrapNode();

private:
    std::unique_ptr<std::thread> announceTimer_;
    void start() override;
    void stop() override;

    void handleAlerts() override;
    
    void initializeHeartbeat();
    std::unique_ptr<BootstrapHeartbeat> heartbeat_manager_;
};

} // namespace torrent_p2p

#endif