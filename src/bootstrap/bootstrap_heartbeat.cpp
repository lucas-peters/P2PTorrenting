#include "bootstrap_heartbeat.hpp"
#include <iostream>

namespace torrent_p2p {

BootstrapHeartbeat::BootstrapHeartbeat(Gossip& gossip, const std::vector<lt::tcp::endpoint>& bootstrap_nodes)
    : gossip_(gossip), bootstrap_nodes_(bootstrap_nodes), running_(false) {
}

BootstrapHeartbeat::~BootstrapHeartbeat() {
    stop();
}

void BootstrapHeartbeat::start() {
    running_ = true;
    heartbeat_thread_ = std::make_unique<std::thread>(&BootstrapHeartbeat::heartbeatLoop, this);
}

void BootstrapHeartbeat::stop() {
    running_ = false;
    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }
}

void BootstrapHeartbeat::addBootstrapNode(const lt::tcp::endpoint& node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Check if node already exists
    auto it = std::find(bootstrap_nodes_.begin(), bootstrap_nodes_.end(), node);
    if (it == bootstrap_nodes_.end()) {
        bootstrap_nodes_.push_back(node);
    }
}

void BootstrapHeartbeat::removeBootstrapNode(const lt::tcp::endpoint& node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = std::find(bootstrap_nodes_.begin(), bootstrap_nodes_.end(), node);
    if (it != bootstrap_nodes_.end()) {
        bootstrap_nodes_.erase(it);
    }
    
    // Remove from last heartbeat tracking
    last_heartbeat_.erase(node);
}

bool BootstrapHeartbeat::isBootstrapNodeAlive(const lt::tcp::endpoint& node) const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto heartbeat_it = last_heartbeat_.find(node);
    if (heartbeat_it == last_heartbeat_.end()) {
        // Node not tracked yet, consider it potentially alive
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    return (now - heartbeat_it->second) < NODE_TIMEOUT;
}

void BootstrapHeartbeat::heartbeatLoop() {
    while (running_) {
        try {
            // Send heartbeats to all bootstrap nodes
            sendHeartbeats();
            
            // Check health of nodes
            checkNodeHealth();
            
            // Sleep for heartbeat interval
            std::this_thread::sleep_for(HEARTBEAT_INTERVAL);
        } catch (const std::exception& e) {
            std::cerr << "Heartbeat loop error: " << e.what() << std::endl;
        }
    }
}

void BootstrapHeartbeat::sendHeartbeats() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    for (const auto& node : bootstrap_nodes_) {
        // Create a heartbeat message
        GossipMessage heartbeat_msg;
        heartbeat_msg.set_timestamp(std::time(nullptr));
        heartbeat_msg.set_message_id("heartbeat_" + std::to_string(std::time(nullptr)));
        
        // Add a heartbeat-specific field to the message
        HeartbeatMessage* heartbeat = heartbeat_msg.mutable_heartbeat();
        heartbeat->set_type(HeartbeatMessage::PING);
        heartbeat->set_timestamp(std::time(nullptr));
        
        // Send the heartbeat message using the new public method
        gossip_.sendMessage(node, heartbeat_msg);
    }
}

void BootstrapHeartbeat::processHeartbeatResponse(const lt::tcp::endpoint& sender) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Update the last heartbeat time for the sender
    last_heartbeat_[sender] = std::chrono::steady_clock::now();
}

void BootstrapHeartbeat::checkNodeHealth() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<lt::tcp::endpoint> failed_nodes;
    
    // Check each tracked node's last heartbeat
    for (const auto& node : bootstrap_nodes_) {
        auto heartbeat_it = last_heartbeat_.find(node);
        if (heartbeat_it == last_heartbeat_.end()) {
            // Node hasn't been heard from, mark as potentially failed
            failed_nodes.push_back(node);
        } else if ((now - heartbeat_it->second) >= NODE_TIMEOUT) {
            // Node hasn't responded within timeout period
            failed_nodes.push_back(node);
            std::cerr << "Bootstrap node failed: " << node.address() << ":" << node.port() << std::endl;
        }
    }
    
    // Remove failed nodes from the bootstrap nodes list
    for (const auto& failed_node : failed_nodes) {
        auto it = std::find(bootstrap_nodes_.begin(), bootstrap_nodes_.end(), failed_node);
        if (it != bootstrap_nodes_.end()) {
            bootstrap_nodes_.erase(it);
        }
    }
}

} // namespace torrent_p2p