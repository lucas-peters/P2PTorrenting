#include "gossip/index_heartbeat.hpp"
#include <iostream>

namespace torrent_p2p {

IndexHeartbeat::IndexHeartbeat(Gossip& gossip, const std::vector<lt::tcp::endpoint>& index_nodes, 
                               const std::string& ip, const int port)
    : BootstrapHeartbeat(gossip, index_nodes, ip, port) {
    std::cout << "[IndexHeartbeat] Initializing with " << index_nodes.size() << " index nodes" << std::endl;
}

IndexHeartbeat::~IndexHeartbeat() {
    // Base class destructor will handle stopping the heartbeat thread
}

void IndexHeartbeat::sendHeartbeats() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::cout << "[IndexHeartbeat] Sending heartbeats to " << nodes_.size() << " nodes" << std::endl;
    
    // Create a heartbeat message
    GossipMessage heartbeat_msg;
    heartbeat_msg.set_timestamp(std::time(nullptr));
    heartbeat_msg.set_message_id("index_heartbeat_" + std::to_string(std::time(nullptr)));
    heartbeat_msg.set_source_ip(ip_);
    heartbeat_msg.set_source_port(port_);
    
    // Add a heartbeat-specific field to the message
    HeartbeatMessage* heartbeat = heartbeat_msg.mutable_heartbeat();
    heartbeat->set_type(HeartbeatMessage::INDEX_PING);
    heartbeat->set_timestamp(std::time(nullptr));
    
    // Send the heartbeat message, use null endpoint since we dont want to exclude any nodes
    gossip_.spreadMessage(heartbeat_msg, lt::tcp::endpoint());
}

void IndexHeartbeat::processHeartbeatResponse(const lt::tcp::endpoint& sender) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Update the last heartbeat time for the sender
    last_heartbeat_[sender] = std::chrono::steady_clock::now();
    std::cout << "[IndexHeartbeat] Processed heartbeat response from: " 
              << sender.address().to_string() << ":" << sender.port() << std::endl;
    
    // If this is a new index node, add it to our list
    auto it = std::find(nodes_.begin(), nodes_.end(), sender);
    if (it == nodes_.end()) {
        nodes_.push_back(sender);
        std::cout << "[IndexHeartbeat] Added new index node from heartbeat response: " 
                  << sender.address().to_string() << ":" << sender.port() << std::endl;
    }
}

std::vector<lt::tcp::endpoint> IndexHeartbeat::getAliveIndexNodes() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<lt::tcp::endpoint> alive_nodes;
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& node : nodes_) {
        auto heartbeat_it = last_heartbeat_.find(node);
        if (heartbeat_it == last_heartbeat_.end() || 
            (now - heartbeat_it->second) < NODE_TIMEOUT) {
            alive_nodes.push_back(node);
        }
    }
    
    std::cout << "[IndexHeartbeat] Found " << alive_nodes.size() << " alive nodes out of " 
              << nodes_.size() << " total nodes" << std::endl;
    
    return alive_nodes;
}

} // namespace torrent_p2p