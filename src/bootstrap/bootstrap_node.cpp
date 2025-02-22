#include "bootstrap_node.h"
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/socket.hpp>
#include <iostream>
#include <thread>
#include <chrono>

namespace torrent_p2p {

BootstrapNode::BootstrapNode(int port) : port_(port) {
    initializeSession();
}

BootstrapNode::~BootstrapNode() {
    stop();
}

void BootstrapNode::initializeSession() {
    lt::settings_pack pack;
    pack.set_int(lt::settings_pack::alert_mask, 
        lt::alert::dht_notification
        | lt::alert::port_mapping_notification
        | lt::alert::status_notification
        | lt::alert::error_notification);
    
    // Enable DHT
    pack.set_bool(lt::settings_pack::enable_dht, true);
    
    // Only listen on localhost
    pack.set_str(lt::settings_pack::listen_interfaces, "127.0.0.1:" + std::to_string(port_));
    
    session_ = std::make_unique<lt::session>(pack);
}

void BootstrapNode::start() {
    if (!session_) {
        initializeSession();
    }
    
    // DHT is automatically started when enabled in settings
    
    // Start handling alerts in a separate thread
    std::thread([this]() {
        while (session_) {
            handleAlerts();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
    
    std::cout << "Bootstrap node started on port " << port_ << std::endl;
}

void BootstrapNode::stop() {
    if (session_) {
        // DHT will be automatically stopped when session is destroyed
        session_.reset();
    }
}

void BootstrapNode::handleAlerts() {
    std::vector<lt::alert*> alerts;
    session_->pop_alerts(&alerts);

    for (lt::alert* a : alerts) {
        if (auto* dht_stats = lt::alert_cast<lt::dht_stats_alert>(a)) {
            std::cout << "[Bootstrap] DHT stats - ";
            for (auto &it : dht_stats->routing_table) {
                std::cout << it.num_nodes << std::endl;
            }
        }
        else if (auto* dht_bootstrap = lt::alert_cast<lt::dht_bootstrap_alert>(a)) {
            std::cout << "[Bootstrap] DHT bootstrap completed. Node ready for connections." << std::endl;
        }
        else if (auto* listen_failed = lt::alert_cast<lt::listen_failed_alert>(a)) {
            std::cerr << "[Bootstrap] Listen failed on port " << listen_failed->port
                      << ": " << listen_failed->error.message() << std::endl;
        }
        else if (auto* listen_succeeded = lt::alert_cast<lt::listen_succeeded_alert>(a)) {
            std::cout << "[Bootstrap] Successfully listening on port " << listen_succeeded->port << std::endl;
        }
        else {
            // Log all alert messages for debugging
            std::cout << a->message() << std::endl;
        }
    }
}

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

} // namespace torrent_p2p
