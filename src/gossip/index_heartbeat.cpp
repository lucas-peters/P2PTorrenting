// File: /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/gossip/index_heartbeat.cpp

#include "gossip/index_heartbeat.hpp"
#include <iostream>

namespace torrent_p2p {

IndexHeartbeat::IndexHeartbeat(Gossip& gossip, const std::vector<lt::tcp::endpoint>& index_nodes)
    : gossip_(gossip), index_nodes_(index_nodes), running_(false) {
}

IndexHeartbeat::~IndexHeartbeat() {
    stop();
}

void IndexHeartbeat::start() {
    running_ = true;
    heartbeat_thread_ = std::make_unique<std::thread>(&IndexHeartbeat::heartbeatLoop, this);
}

void IndexHeartbeat::stop() {
    running_ = false;
    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }
}

void IndexHeartbeat::addIndexNode(const lt::tcp::endpoint& node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Check if node already exists
    auto it = std::find(index_nodes_.begin(), index_nodes_.end(), node);
    if (it == index_nodes_.end()) {
        index_nodes_.push_back(node);
        std::cout << "Added index node: " << node.address().to_string() << ":" << node.port() << std::endl;
    }
}

void IndexHeartbeat::removeIndexNode(const lt::tcp::endpoint& node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = std::find(index_nodes_.begin(), index_nodes_.end(), node);
    if (it != index_nodes_.end()) {
        index_nodes_.erase(it);
        std::cout << "Removed index node: " << node.address().to_string() << ":" << node.port() << std::endl;
    }
    
    // Remove from last heartbeat tracking
    last_heartbeat_.erase(node);
}

bool IndexHeartbeat::isIndexNodeAlive(const lt::tcp::endpoint& node) const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto heartbeat_it = last_heartbeat_.find(node);
    if (heartbeat_it == last_heartbeat_.end()) {
        // Node not tracked yet, consider it potentially alive
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    return (now - heartbeat_it->second) < NODE_TIMEOUT;
}

std::vector<lt::tcp::endpoint> IndexHeartbeat::getAliveIndexNodes() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<lt::tcp::endpoint> alive_nodes;
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& node : index_nodes_) {
        auto heartbeat_it = last_heartbeat_.find(node);
        if (heartbeat_it == last_heartbeat_.end() || 
            (now - heartbeat_it->second) < NODE_TIMEOUT) {
            alive_nodes.push_back(node);
        }
    }
    
    return alive_nodes;
}

void IndexHeartbeat::heartbeatLoop() {
    while (running_) {
        try {
            // Send heartbeats to all index nodes
            sendHeartbeats();
            
            // Check health of nodes
            checkNodeHealth();
            
            // Sleep for heartbeat interval
            std::this_thread::sleep_for(HEARTBEAT_INTERVAL);
        } catch (const std::exception& e) {
            std::cerr << "Index heartbeat loop error: " << e.what() << std::endl;
        }
    }
}

void IndexHeartbeat::sendHeartbeats() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    for (const auto& node : index_nodes_) {
        // Create a heartbeat message
        GossipMessage heartbeat_msg;
        heartbeat_msg.set_timestamp(std::time(nullptr));
        heartbeat_msg.set_message_id("index_heartbeat_" + std::to_string(std::time(nullptr)));
        
        // Add a heartbeat-specific field to the message
        HeartbeatMessage* heartbeat = heartbeat_msg.mutable_heartbeat();
        heartbeat->set_type(HeartbeatMessage::INDEX_PING);
        heartbeat->set_timestamp(std::time(nullptr));
        
        // Send the heartbeat message
        gossip_.sendMessage(node, heartbeat_msg);
    }
}

void IndexHeartbeat::processHeartbeatResponse(const lt::tcp::endpoint& sender) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Update the last heartbeat time for the sender
    last_heartbeat_[sender] = std::chrono::steady_clock::now();
    
    // If this is a new index node, add it to our list
    auto it = std::find(index_nodes_.begin(), index_nodes_.end(), sender);
    if (it == index_nodes_.end()) {
        index_nodes_.push_back(sender);
        std::cout << "Discovered new index node: " << sender.address().to_string() << ":" << sender.port() << std::endl;
        
        // Request index sync with the new node
        requestIndexSync(sender);
    }
}

void IndexHeartbeat::requestIndexSync(const lt::tcp::endpoint& target) {
    // Create a sync request message
    GossipMessage sync_msg;
    sync_msg.set_timestamp(std::time(nullptr));
    sync_msg.set_message_id("index_sync_" + std::to_string(std::time(nullptr)));
    
    // Add an index sync field to the message
    IndexSyncMessage* sync = sync_msg.mutable_index_sync();
    sync->set_type(IndexSyncMessage::REQUEST_FULL);
    sync->set_timestamp(std::time(nullptr));
    
    // Send the sync message
    gossip_.sendMessage(target, sync_msg);
    std::cout << "Requested index sync with: " << target.address().to_string() << ":" << target.port() << std::endl;
}

void IndexHeartbeat::checkNodeHealth() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<lt::tcp::endpoint> failed_nodes;
    
    // Check each tracked node's last heartbeat
    for (const auto& node : index_nodes_) {
        auto heartbeat_it = last_heartbeat_.find(node);
        if (heartbeat_it != last_heartbeat_.end() && 
            (now - heartbeat_it->second) >= NODE_TIMEOUT) {
            // Node hasn't responded within timeout period
            failed_nodes.push_back(node);
            std::cerr << "Index node failed: " << node.address().to_string() << ":" << node.port() << std::endl;
        }
    }
    
    // Remove failed nodes from the index nodes list
    for (const auto& failed_node : failed_nodes) {
        auto it = std::find(index_nodes_.begin(), index_nodes_.end(), failed_node);
        if (it != index_nodes_.end()) {
            index_nodes_.erase(it);
        }
    }
}

} // namespace torrent_p2p