#include "bootstrap_node.hpp"


namespace torrent_p2p {

BootstrapNode::BootstrapNode(int port, const std::string& env, const std::string& ip) : Node(port, env, ip) {
    start();
}

BootstrapNode::BootstrapNode(int port, const std::string& env, const std::string& ip, const std::string& state_file) : Node(port, env, state_file, ip) {
    start();
}

BootstrapNode::~BootstrapNode() {
    stop();
}

void BootstrapNode::start() {
    Node::start();
        
    // makes announcements to the dht so that nodes
    announceTimer_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            try {
                if (session_) {
                    std::cout << "[Bootstrap] Requesting DHT stats..." << std::endl;
                    session_->post_dht_stats();
                    
                    // using this to help discover peers faster
                    session_->dht_get_peers(lt::sha1_hash());
                    
                    // use a hash to announce
                    lt::sha1_hash hash;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<uint32_t> dis;
                    for (int i = 0; i < 5; i++) {
                        reinterpret_cast<uint32_t*>(hash.data())[i] = dis(gen);
                    }
                    std::cout << "[Bootstrap] Announcing hash: " << hash << " on port " << port_ << std::endl;
                    session_->dht_announce(hash, port_);
                    
                    // prints out routing table
                    lt::session_params params = session_->session_state();
                    std::cout << "[Bootstrap] DHT routing table has " << params.dht_state.nodes.size() << " nodes" << std::endl;
                    for (auto & node : params.dht_state.nodes) {
                        std::cout << "[Bootstrap] DHT node: " << node.address() << ":" << node.port() << std::endl;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[Bootstrap] Error in announce timer: " << e.what() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
    
    
    running_ = true;
    
    try {
        // heartbeat started here
        initializeHeartbeat();
    } catch (const std::exception& e) {
        std::cerr << "[Bootstrap] Exception during Heartbeat initialization: " << e.what() << std::endl;
    }
    
    // handle alerts thread
    std::thread([this]() {
        while (running_) {
            try {
                handleAlerts();
            } catch (const std::exception& e) {
                std::cerr << "[Bootstrap] Error handling alerts: " << e.what() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
    
    std::cout << "[Bootstrap] Node fully started and running" << std::endl;
}

void BootstrapNode::stop() {
    running_ = false;
    if (announceTimer_ && announceTimer_->joinable()) {
        announceTimer_->join();
    }
    if (gossip_) {
        gossip_->stop();
    }
    if (heartbeat_manager_) {
        heartbeat_manager_->stop();
    }
    saveDHTState();
    if (session_) {
        session_.reset();
    }
}

void BootstrapNode::handleAlerts() {
    if (!session_) return;

    std::vector<lt::alert*> alerts;
    session_->pop_alerts(&alerts);

    for (lt::alert* a : alerts) {
        try {
            if (auto* dht_stats = lt::alert_cast<lt::dht_stats_alert>(a)) {
                int total_nodes = 0;
                for (auto const& t : dht_stats->routing_table) {
                    total_nodes += t.num_nodes;
                }
                std::cout << "\n[Bootstrap] DHT nodes in routing table: " << total_nodes << std::endl;
            } else if (auto* node_alert = lt::alert_cast<lt::dht_bootstrap_alert>(a)) {
                std::cout << "[Bootstrap] DHT bootstrap complete" << std::endl;
            } else if (auto* listen_failed = lt::alert_cast<lt::listen_failed_alert>(a)) {
                std::cerr << "[Bootstrap] Listen failed on port " << listen_failed->port
                          << ": " << listen_failed->error.message() << std::endl;
            } else if (auto* listen_succeeded = lt::alert_cast<lt::listen_succeeded_alert>(a)) {
                std::cout << "[Bootstrap] Successfully listening on port " << listen_succeeded->port << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Bootstrap] Error processing alert: " << e.what() << std::endl;
        }
    }
}

void BootstrapNode::initializeHeartbeat() {
    std::cout << "[Bootstrap] Initializing Heartbeat" << std::endl;
    if (!gossip_) {
        std::cerr << "[Bootstrap] Error: Cannot initialize heartbeat, gossip_ is null" << std::endl;
        return;
    }
    gossip_->setHeartbeatHandler([this](const lt::tcp::endpoint& sender) {
        std::cout << "[Bootstrap] Received heartbeat response from: " 
                  << sender.address() << ":" << sender.port() << std::endl;
        
        if (heartbeat_manager_) {
            heartbeat_manager_->processHeartbeatResponse(sender);
        }
    });
    
    if (!bootstrap_nodes_.empty()) {
        // Convert bootstrap_nodes_ from std::vector<std::pair<std::string, int>> to std::vector<lt::tcp::endpoint>
        std::vector<lt::tcp::endpoint> endpoint_nodes;
        for (const auto& node : bootstrap_nodes_) {
            try {
                lt::tcp::endpoint endpoint(lt::make_address_v4(node.first), node.second + 1000);
                endpoint_nodes.push_back(endpoint);
                std::cout << "[Bootstrap] Added endpoint: " << node.first << ":" << node.second << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[Bootstrap] Error creating endpoint for " << node.first << ":" << node.second 
                          << " - " << e.what() << std::endl;
            }
        }
        
        // create heartbeat manager
        try {
            heartbeat_manager_ = std::make_unique<BootstrapHeartbeat>(*gossip_, endpoint_nodes, ip_, port_ + 1000);
            std::cout << "[Bootstrap] Heartbeat manager created successfully" << std::endl;
            
            // start heartbeating
            heartbeat_manager_->start();
            std::cout << "[Bootstrap] Heartbeat started successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Bootstrap] Error creating heartbeat manager: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "[Bootstrap] Cannot initialize heartbeat - no bootstrap nodes" << std::endl;
    }
}

} // namespace torrent_p2p
