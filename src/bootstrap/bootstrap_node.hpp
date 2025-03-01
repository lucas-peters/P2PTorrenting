#ifndef BOOTSTRAP_NODES_H
#define BOOTSTRAP_NODES_H

#include "node/node.hpp"

#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert_types.hpp>
#include <string>
#include <memory>
#include <vector>

namespace torrent_p2p {

class BootstrapNode : public Node {
public:
    BootstrapNode(int port = 6881);
    BootstrapNode(int port, const std::string& state_file);
    ~BootstrapNode();

private:
    std::unique_ptr<std::thread> announceTimer_;
    void start() override;
    void stop() override;
    // Handle DHT alerts
    void handleAlerts() override;
};

} // namespace torrent_p2p

#endif