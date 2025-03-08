#include "bootstrap_node.hpp"


namespace torrent_p2p {

BootstrapNode::BootstrapNode(int port) : Node(port) {
    start();
}

BootstrapNode::BootstrapNode(int port, const std::string& state_file) : Node(port, state_file) {
    start();
    // The base class constructor already loads the DHT state, so we don't need to call loadDHTState again
}

BootstrapNode::BootstrapNode(int port, const std::vector<lt::tcp::endpoint>& bootstrap_nodes): Node(port), bootstrap_nodes_(bootstrap_nodes) {
    start();
}

BootstrapNode::~BootstrapNode() {
    stop();
}

void BootstrapNode::start() {
    if (!session_) {
        lt::settings_pack pack;
        pack.set_int(lt::settings_pack::alert_mask, 
            lt::alert::dht_notification + 
            lt::alert::status_notification +
            lt::alert::error_notification +
            lt::alert::dht_log_notification);
        
        // Force Highest Level Encryption, ChaCha20 instead of RC4
        pack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_forced);
        pack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_forced);
            
        // Enable DHT with local-only settings
        pack.set_bool(lt::settings_pack::enable_dht, true);
        pack.set_str(lt::settings_pack::dht_bootstrap_nodes, "");  // Disable external bootstrap nodes
        pack.set_int(lt::settings_pack::dht_announce_interval, 5);
        pack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
        pack.set_bool(lt::settings_pack::enable_incoming_utp, true);
        pack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
        pack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
        
        // Disable IP restrictions for local testing, by default it blocks having 2 nodes in the routing table from the same ip
        pack.set_bool(lt::settings_pack::dht_restrict_routing_ips, false);
        pack.set_bool(lt::settings_pack::dht_restrict_search_ips, false);
        pack.set_int(lt::settings_pack::dht_max_peers_reply, 100);
        pack.set_bool(lt::settings_pack::dht_ignore_dark_internet, false);
        pack.set_int(lt::settings_pack::dht_max_fail_count, 100);  // More forgiving of failures
        
        // Listen on all interfaces to accept connections from any IP
        pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:" + std::to_string(port_));        
        std::cout << "[Bootstrap] Starting on port " << port_ << " listening on all interfaces" << std::endl;
        
        // Set up the session
        session_ = std::make_unique<lt::session>(pack);
        
        // Verify that we're actually listening
        // auto endpoints = session_->get_listen_status();
        // if (endpoints.empty()) {
        //     std::cerr << "[Bootstrap] WARNING: Not listening on any interfaces!" << std::endl;
        // } else {
        //     for (const auto& ep : endpoints) {
        //         std::cout << "[Bootstrap] Listening on: " << ep.address() << ":" << ep.port() << std::endl;
        //     }
        // }
        
        // Start periodic DHT announcements
        announceTimer_ = std::make_unique<std::thread>([this]() {
            while (running_) {
                try {
                    if (session_) {
                        std::cout << "[Bootstrap] Requesting DHT stats..." << std::endl;
                        session_->post_dht_stats();
                        
                        // Force DHT bootstrap
                        session_->dht_get_peers(lt::sha1_hash());
                        
                        // Announce ourselves with a generated hash
                        lt::sha1_hash hash;
                        std::random_device rd;
                        std::mt19937 gen(rd());
                        std::uniform_int_distribution<uint32_t> dis;
                        for (int i = 0; i < 5; i++) {
                            reinterpret_cast<uint32_t*>(hash.data())[i] = dis(gen);
                        }
                        std::cout << "[Bootstrap] Announcing hash: " << hash << " on port " << port_ << std::endl;
                        session_->dht_announce(hash, port_);
                        
                        // Print current DHT routing table
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
    }
    
    running_ = true;
    
    try {
        std::cout << "[Bootstrap] Creating Gossip object..." << std::endl;
        gossip_ = std::make_unique<torrent_p2p::Gossip>(*session_, port_ + 1000);
        std::cout << "[Bootstrap] Gossip object created successfully" << std::endl;
    
        // Ensure Heartbeat Manager is initialized
        initializeHeartbeat();
    
        if (!heartbeat_manager_) {
            std::cerr << "Error: Heartbeat manager failed to start!" << std::endl;
        } else {
            std::cout << "[Bootstrap] Heartbeat Manager is running!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Bootstrap] Exception during Gossip initialization: " << e.what() << std::endl;
    }
     catch (const std::exception& e) {
        std::cerr << "[Bootstrap] Exception during Gossip initialization: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[Bootstrap] Unknown exception during Gossip initialization" << std::endl;
    }
    
    // Start handling alerts in a separate thread
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
                
                // Print detailed routing table info
                if (total_nodes > 0) {
                    std::cout << "[Bootstrap] Routing table details:" << std::endl;
                    for (size_t i = 0; i < dht_stats->routing_table.size(); ++i) {
                        const auto& bucket = dht_stats->routing_table[i];
                        if (bucket.num_nodes > 0) {
                            std::cout << "  Bucket " << i << ": " << bucket.num_nodes << " nodes" << std::endl;
                        }
                    }
                }
            } else if (auto* dht_log = lt::alert_cast<lt::dht_log_alert>(a)) {
                // Log all DHT activity for debugging
                std::cout << "[Bootstrap] DHT: " << dht_log->message() << std::endl;
            } else if (auto* node_alert = lt::alert_cast<lt::dht_bootstrap_alert>(a)) {
                std::cout << "[Bootstrap] DHT bootstrap complete" << std::endl;
            } else if (auto* listen_failed = lt::alert_cast<lt::listen_failed_alert>(a)) {
                std::cerr << "[Bootstrap] Listen failed on port " << listen_failed->port
                          << ": " << listen_failed->error.message() << std::endl;
            } else if (auto* listen_succeeded = lt::alert_cast<lt::listen_succeeded_alert>(a)) {
                std::cout << "[Bootstrap] Successfully listening on port " << listen_succeeded->port << std::endl;
            // } else if (auto* pa = lt::alert_cast<lt::peer_connect_alert>(a)) {
            // // We are ensuring that all peers are using ChaCha20 encryption
            //     std::vector<lt::peer_info> peers;
            //     pa->handle.get_peer_info(peers);
            //     for (const auto& p : peers) {
            //         if (p.ip == pa->ip) { // Find the peer that just connected
            //             if (p.flags & lt::peer_info::rc4) {
            //                 std::cout << "Peer " << p.ip << " is using RC4 encryption." << std::endl; // Should NEVER happen
            //             } else {
            //                 std::cout << "Peer " << p.ip << " is using ChaCha20 encryption." << std::endl; // Expected output
            //             }
            //         }
            //     }
            // 
            } else {
                // Log all alert messages for debugging
                std::cout << a->message() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Bootstrap] Error processing alert: " << e.what() << std::endl;
        }
    }
}

BootstrapHeartbeat* BootstrapNode::getHeartbeatManager() {
    return heartbeat_manager_.get();
}

void BootstrapNode::initializeHeartbeat() {
    std::cout << "[Bootstrap] Initializing Heartbeat..." << std::endl;

    if (!gossip_) {
        std::cerr << "Error: Gossip is not initialized!" << std::endl;
        return;
    }

    if (!heartbeat_manager_) {
        heartbeat_manager_ = std::make_unique<BootstrapHeartbeat>(*gossip_, bootstrap_nodes_);
        heartbeat_manager_->start();
        std::cout << "[Bootstrap] Heartbeat Manager started!" << std::endl;
    } else {
        std::cout << "[Bootstrap] Heartbeat Manager already running." << std::endl;
    }
}



} // namespace torrent_p2p
