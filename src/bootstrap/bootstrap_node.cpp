#include "bootstrap_node.h"
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/socket.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

namespace torrent_p2p {

BootstrapNode::BootstrapNode(int port) : port_(port), running_(false) {}

BootstrapNode::~BootstrapNode() {
    stop();
}

void BootstrapNode::start() {
    if (!session_) {
        lt::settings_pack pack;
        pack.set_int(lt::settings_pack::alert_mask, 
            lt::alert::dht_notification | 
            lt::alert::status_notification |            
            lt::alert::error_notification |
            lt::alert::dht_log_notification);
        
        // Force Highest Level Encryption, ChaCha20 instead of RC4
        pack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_forced);
        pack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_forced);
        
        // Enable DHT
        pack.set_bool(lt::settings_pack::enable_dht, true);
        pack.set_str(lt::settings_pack::dht_bootstrap_nodes, "");  // Disable external bootstrap nodes
        pack.set_int(lt::settings_pack::dht_announce_interval, 5);
        //pack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
        //pack.set_bool(lt::settings_pack::enable_incoming_utp, true);
        //pack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
        //pack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
        
        // Disable IP restrictions for local testing, by default it blocks having 2 nodes in the routing table from the same ip
        pack.set_bool(lt::settings_pack::dht_restrict_routing_ips, false);
        pack.set_bool(lt::settings_pack::dht_restrict_search_ips, false);
        pack.set_int(lt::settings_pack::dht_max_peers_reply, 100);
        pack.set_bool(lt::settings_pack::dht_ignore_dark_internet, false);
        pack.set_int(lt::settings_pack::dht_max_fail_count, 5); // After 5 failed heartbeats we determin the node to be dead and remove it
        
        // Only listen on localhost, need to change this for deployment
        pack.set_str(lt::settings_pack::listen_interfaces, "127.0.0.1:" + std::to_string(port_));
        
        std::cout << "[Bootstrap] Starting on port " << port_ << std::endl;
        session_ = std::make_unique<lt::session>(pack);
        
        // Register the gossip extension
        register_gossip_extension(*session_);
        
        // Start a thread to announce our presence periodically, ensures other nodes don't remove this from dht
        // Simulating activity for testing purposes, would not be used in a real deployment
        announceTimer_ = std::make_unique<std::thread>([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                
                if (session_) {
                    std::cout << "[Bootstrap] Requesting DHT stats..." << std::endl;
                    session_->post_dht_stats();
                    
                    // Announce our presence
                    session_->dht_get_peers(lt::sha1_hash());
                    
                    // Announce random hashes to keep the DHT active
                    for (int i = 0; i < 5; ++i) {
                        lt::sha1_hash hash;
                        for (int j = 0; j < 20; ++j) {
                            hash[j] = lt::random(256) & 0xff;
                        }
                        session_->dht_announce(hash, port_);
                    }
                }
            }
        });
    }
    
    running_ = true;
    
    // Start a thread to handle alerts
    std::thread([this]() {
        while (running_) {
            handleAlerts();
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
    if (session_) {
        session_->post_dht_stats();
        // DHT will be automatically stopped when session is destroyed
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
            } else {
                // Log all alert messages for debugging
                std::cout << a->message() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Bootstrap] Error processing alert: " << e.what() << std::endl;
        }
    }
}

<<<<<<< Updated upstream
std::string BootstrapNode::getDHTStats() const {
    if (!session_) {
        return "Session not initialized";
    }
    
    session_->post_dht_stats();
    return "DHT stats requested";
}

std::pair<std::string, int> BootstrapNode::getEndpoint() const {
    if (!session_) {
        return {"not started", 0};
    }
    
    return {"127.0.0.1", port_};
}

=======
>>>>>>> Stashed changes
} // namespace torrent_p2p
