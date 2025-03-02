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


namespace torrent_p2p {

// Node is an abstract class that defines the interface for other nodes to inherit from
// It implements common functions for nodes, to improve code reusability

class Node {
public:
    Node(int port = 6881);
    // Constructor to load from save state file
    Node(int port, const std::string& state_file);
    virtual ~Node();

    // Get the node's DHT state
    std::string getDHTStats() const;
    // Get the node's endpoint information
    std::pair<std::string, int> getEndpoint() const;
    // Save/load the DHT state to/from a file
    bool saveDHTState() const;
    bool loadDHTState(const std::string& state_file);
    
    // Reputation management
    void updatePeerReputation(const std::string& peer_id, std::int64_t delta);
    std::int64_t getPeerReputation(const std::string& peer_id) const;

private:
    // Start the node, needs to be implemented slightly different for each type of node
    virtual void start() = 0;
    
    // Stop the node
    virtual void stop();
    
    // Handle DHT alerts, each node will implement this differently
    virtual void handleAlerts() = 0;

protected:
    std::unique_ptr<lt::session> session_;
    int port_;
    bool running_;
    std::string state_file_;

    //GossipExtension gossip_extension_;
    //GossipPeerPlugin gossip_peer_plugin_;
}; // namespace torrent_p2p

} // namespace torrent_p2p

#endif