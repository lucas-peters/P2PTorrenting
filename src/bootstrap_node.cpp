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
        | lt::alert::error_notification
        | lt::alert::status_notification);
    
    // Enable DHT
    pack.set_bool(lt::settings_pack::enable_dht, true);
    pack.set_str(lt::settings_pack::dht_bootstrap_nodes, "dht.libtorrent.org:25401");
    
    // Set the listen port range
    pack.set_int(lt::settings_pack::dht_upload_rate_limit, 20000);
    pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:" + std::to_string(port_));
    
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
            // Handle DHT statistics
            std::cout << "DHT running" << std::endl;
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
