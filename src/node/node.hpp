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
#include <sstream>
#include <cstdlib>
#include <nlohmann/json.hpp>

#include "index/messenger.hpp"
#include "gossip/gossip.hpp"
#include "gossip.pb.h"

namespace torrent_p2p {

// Node is an abstract class that defines the interface for other nodes to inherit from
// It implements common functions for nodes, to improve code reusability

class Node {
public:
    Node(int port = 6881, const std::string& env = "aws");

    // Constructor to load from save state file
    Node(int port, const std::string& env, const std::string& state_file);
    ~Node();
    // Get the node's DHT state
    std::string getDHTStats() const;
    // Get the node's endpoint information
    std::pair<std::string, int> getEndpoint() const;

    // Save/load the DHT state to/from a file
    bool saveDHTState() const;
    bool loadDHTState(const std::string& state_file);
    
private:
    
    // Handle DHT alerts, each node will implement this differently
    virtual void handleAlerts() = 0;
    void loadEnvConfig(const std::string& env);

protected:
    std::unique_ptr<lt::session> session_;
    std::unique_ptr<Gossip> gossip_;
    std::unique_ptr<Messenger> messenger_;
    
    int port_;
    bool running_;
    std::string state_file_;
    // Predefined list of static endpoints set in config.json, loaded on start up based on --env flag
    std::vector<std::pair<std::string, int>> bootstrap_nodes_;
    std::vector<std::pair<std::string, int>> index_nodes_;

    // All Node subtypes should be started using these
    virtual void start();
        // Stop the node
    virtual void stop();
    void connectToDHT(const std::vector<std::pair<std::string, int>>& bootstrap_nodes);
    void initializeSession();

}; // namespace torrent_p2p

} // namespace torrent_p2p

#endif