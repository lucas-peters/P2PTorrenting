#ifndef BOOTSTRAP_NODES_H
#define BOOTSTRAP_NODES_H

#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert_types.hpp>
#include <string>
#include <memory>
#include <vector>

namespace torrent_p2p {

class BootstrapNode {
public:
    BootstrapNode(int port = 6881);
    ~BootstrapNode();

    // Start the bootstrap node
    void start();
    
    // Stop the bootstrap node
    void stop();
    
    // Get the node's DHT state
    std::string getDHTStats() const;
    
    // Get the node's endpoint information
    std::pair<std::string, int> getEndpoint() const;

private:
    std::unique_ptr<lt::session> session_;
    std::unique_ptr<std::thread> announceTimer_;
    int port_;
    bool running_;
    
    // Initialize the session with DHT settings
    void initializeSession();
    
    // Handle DHT alerts
    void handleAlerts();
};

} // namespace torrent_p2p

#endif