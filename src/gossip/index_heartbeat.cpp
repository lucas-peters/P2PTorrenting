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

void IndexHeartbeat::requestIndexSync(const lt::tcp::endpoint& target) {
    std::cout << "[IndexHeartbeat] Requesting index sync from: " 
              << target.address().to_string() << ":" << target.port() << std::endl;
    // Create a sync request message
    GossipMessage sync_msg;
    sync_msg.set_timestamp(std::time(nullptr));
    sync_msg.set_message_id("index_sync_" + std::to_string(std::time(nullptr)));
    
    // Add an index sync field to the message
    IndexSyncMessage* sync = sync_msg.mutable_index_sync();
    sync->set_type(IndexSyncMessage::REQUEST_FULL);
    sync->set_timestamp(std::time(nullptr));
    
    // Send the sync message
    gossip_.spreadMessage(sync_msg, target);
    std::cout << "[IndexHeartbeat] Requested index sync with: " << target.address().to_string() << ":" << target.port() << std::endl;
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

// void IndexHeartbeat::checkNodeHealth() {
//     std::lock_guard<std::mutex> lock(nodes_mutex_);
    
//     auto now = std::chrono::steady_clock::now();
//     std::vector<lt::tcp::endpoint> failed_nodes;

//     std::cout << "===== Current heartbeat status =====" << std::endl;
//     for (const auto& entry : last_heartbeat_) {
//         auto time_since_last_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(now - entry.second).count();
//         std::cout << "Node: " << entry.first.address().to_string() << ":" << entry.first.port() 
//                   << " | Last seen: " << time_since_last_heartbeat << " seconds ago" << std::endl;
//     }
//     std::cout << "==================================" << std::endl;
    
//     // Check each tracked node's last heartbeat
//     for (const auto& node : nodes_) {

//         auto heartbeat_it = last_heartbeat_.find(node);
//         // if (heartbeat_it == last_heartbeat_.end()) {
//         //     // Node hasn't been heard from, mark as potentially failed
//         //     // failed_nodes.push_back(node);
//         //     continue;
//         // } else 
//         if ((now - heartbeat_it->second) >= NODE_TIMEOUT) {
//             // Node hasn't responded within timeout period
//             failed_nodes.push_back(node);
//             std::cout << "*** Index node failed: " << node.address() << ":" << node.port() << " ***" << std::endl;
//         }
//     }
    
//     // Remove failed nodes from the bootstrap nodes list
//     for (const auto& failed_node : failed_nodes) {
//         auto it = std::find(nodes_.begin(), nodes_.end(), failed_node);
//         if (it != nodes_.end()) {
//             nodes_.erase(it);
//             // need to rebalance sharding here
//         }
//     }
// }

} // namespace torrent_p2p