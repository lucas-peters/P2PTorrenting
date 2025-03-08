#ifndef NODE_H
#define NODE_H

#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/session_params.hpp>
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <fstream>

#include "gossip/gossip.hpp"
#include "gossip.pb.h"

namespace torrent_p2p {

// Node is an abstract class that defines the interface for other nodes to inherit from
// It implements common functions for nodes, to improve code reusability

class Node {
public:
    Node(int port = 6881);

    // Constructor to load from save state file
    Node(int port, const std::string& state_file);
    ~Node();
    // Get the node's DHT state
    std::string getDHTStats() const;
    // Get the node's endpoint information
    std::pair<std::string, int> getEndpoint() const;

    // Save/load the DHT state to/from a file
    bool saveDHTState() const;
    bool loadDHTState(const std::string& state_file);
    
private:
    // Stop the node
    virtual void stop();
    
    // Handle DHT alerts, each node will implement this differently
    virtual void handleAlerts() = 0;

protected:
    std::unique_ptr<lt::session> session_;
    std::unique_ptr<Gossip> gossip_;
    int port_;
    bool running_;
    std::string state_file_;
    std::vector<std::pair<std::string, int>> bootstrap_nodes_ = {{"172.20.0.2", 6881}};

    // All Node subtypes should be started using these
    virtual void start();
    void connectToDHT(const std::vector<std::pair<std::string, int>>& bootstrap_nodes);
    void initializeSession();

    
    

}; // namespace torrent_p2p

} // namespace torrent_p2p

#endif