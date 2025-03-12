#include "gossip/bootstrap_heartbeat.hpp"
#include <iostream>

namespace torrent_p2p {

BootstrapHeartbeat::BootstrapHeartbeat(Gossip& gossip, const std::vector<lt::tcp::endpoint>& nodes,
     const std::string& ip, const int port): gossip_(gossip), nodes_(nodes), running_(false), ip_(ip), port_(port) {
    std::cout << "CREATED HEARTBEAT OBJECT" << std::endl;
}

BootstrapHeartbeat::~BootstrapHeartbeat() {
    stop();
}

void BootstrapHeartbeat::start() {
    running_ = true;
    self_endpoint_ = lt::tcp::endpoint(lt::make_address_v4(ip_), port_);
    heartbeat_thread_ = std::make_unique<std::thread>(&BootstrapHeartbeat::heartbeatLoop, this);
    std::cout << "HEARTBEAT THREAD STARTED" << std::endl;
}

void BootstrapHeartbeat::stop() {
    running_ = false;
    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }
}

void BootstrapHeartbeat::addNode(const lt::tcp::endpoint& node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Check if node already exists
    auto it = std::find(nodes_.begin(), nodes_.end(), node);
    if (it == nodes_.end()) {
        nodes_.push_back(node);
    }
}

void BootstrapHeartbeat::removeNode(const lt::tcp::endpoint& node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = std::find(nodes_.begin(), nodes_.end(), node);
    if (it != nodes_.end()) {
        nodes_.erase(it);
    }
    
    // Remove from last heartbeat tracking
    last_heartbeat_.erase(node);
}

bool BootstrapHeartbeat::isNodeAlive(const lt::tcp::endpoint& node) const {
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
    std::cout << "Inside heartbeat loop" << std::endl;
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
    std::cout << "----Sending Heartbeat----" << std::endl;
    // Create a heartbeat message
    GossipMessage heartbeat_msg;
    heartbeat_msg.set_timestamp(std::time(nullptr));
    heartbeat_msg.set_message_id("heartbeat_ping_" + ip_ + "_" + std::to_string(port_) + "_" + std::to_string(std::time(nullptr)));
    heartbeat_msg.set_source_ip(ip_);
    heartbeat_msg.set_source_port(port_);
    
    // Add a heartbeat-specific field to the message
    HeartbeatMessage* heartbeat = heartbeat_msg.mutable_heartbeat();
    heartbeat->set_type(HeartbeatMessage::PING);
    heartbeat->set_timestamp(std::time(nullptr));
    
    // Send the heartbeat message using the new public method
    gossip_.spreadMessage(heartbeat_msg, lt::tcp::endpoint());
}

void BootstrapHeartbeat::processHeartbeatResponse(const lt::tcp::endpoint& sender) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);

    std::cout << "received heartbeat from: " << sender.address() << ":" << sender.port() << std::endl;
    
    // Update the last heartbeat time for the sender
    last_heartbeat_[sender] = std::chrono::steady_clock::now();
}

void BootstrapHeartbeat::checkNodeHealth() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<lt::tcp::endpoint> failed_nodes;

    std::cout << "===== Current heartbeat status =====" << std::endl;
    for (const auto& entry : last_heartbeat_) {
        auto time_since_last_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(now - entry.second).count();
        std::cout << "Node: " << entry.first.address().to_string() << ":" << entry.first.port() 
                  << " | Last seen: " << time_since_last_heartbeat << " seconds ago" << std::endl;
    }
    std::cout << "==================================" << std::endl;
    
    // Check each tracked node's last heartbeat
    for (const auto& node : nodes_) {

        auto heartbeat_it = last_heartbeat_.find(node);
        if (heartbeat_it == last_heartbeat_.end()) {
            // Node hasn't been heard from, mark as potentially failed
            // failed_nodes.push_back(node);
            continue;
        } else if ((now - heartbeat_it->second) >= NODE_TIMEOUT) {
            // Node hasn't responded within timeout period
            failed_nodes.push_back(node);
            std::cout << "*** Bootstrap node failed: " << node.address() << ":" << node.port() << " ***" << std::endl;
        }
    }
    
    // Remove failed nodes from the bootstrap nodes list
    for (const auto& failed_node : failed_nodes) {
        auto it = std::find(nodes_.begin(), nodes_.end(), failed_node);
        if (it != nodes_.end()) {
            nodes_.erase(it);
        }
    }
}

} // namespace torrent_p2p