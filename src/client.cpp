#include "client.h"
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/bencode.hpp>
#include <iostream>
#include <thread>
#include <chrono>

namespace torrent_p2p {

Client::Client(int port) : port_(port) {
    initializeSession();
}

Client::~Client() {
    stop();
}

void Client::initializeSession() {
    lt::settings_pack pack;
    pack.set_int(lt::settings_pack::alert_mask,
        lt::alert::status_notification
        | lt::alert::error_notification
        | lt::alert::dht_notification);
    
    // Enable DHT
    pack.set_bool(lt::settings_pack::enable_dht, true);
    pack.set_int(lt::settings_pack::dht_upload_rate_limit, 20000);
    pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:" + std::to_string(port_));
    
    session_ = std::make_unique<lt::session>(pack);
}

void Client::connectToDHT(const std::vector<std::pair<std::string, int>>& bootstrap_nodes) {
    if (!session_) {
        initializeSession();
    }
    
    // Add bootstrap nodes to the DHT
    for (const auto& node : bootstrap_nodes) {
        session_->add_dht_node({node.first, node.second});
    }
}

void Client::start() {
    if (!session_) {
        initializeSession();
    }
    
    // Start handling alerts in a separate thread
    std::thread([this]() {
        while (session_) {
            handleAlerts();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
    
    std::cout << "Client started on port " << port_ << std::endl;
}

void Client::stop() {
    if (session_) {
        // DHT will be automatically stopped when session is destroyed
        session_.reset();
    }
}

void Client::addTorrent(const std::string& torrentFilePath, const std::string& savePath) {
    lt::add_torrent_params params;
    params.save_path = savePath;
    
    try {
        params.ti = std::make_shared<lt::torrent_info>(torrentFilePath);
    } catch (const std::exception& e) {
        std::cerr << "Error loading torrent file: " << e.what() << std::endl;
        return;
    }
    
    lt::torrent_handle h = session_->add_torrent(params);
    torrents_[torrentFilePath] = h;
    
    std::cout << "Added torrent: " << params.ti->name() << std::endl;
}

double Client::getProgress(const std::string& torrentFilePath) {
    auto it = torrents_.find(torrentFilePath);
    if (it == torrents_.end()) {
        return 0.0;
    }
    
    lt::torrent_handle& h = it->second;
    if (!h.is_valid()) {
        return 0.0;
    }
    
    lt::torrent_status status = h.status();
    return status.progress * 100.0;
}

void Client::handleAlerts() {
    std::vector<lt::alert*> alerts;
    session_->pop_alerts(&alerts);

    for (lt::alert* a : alerts) {
        if (auto* dht_stats = lt::alert_cast<lt::dht_stats_alert>(a)) {
            std::cout << "DHT running" << std::endl;
        }
        else if (auto* state = lt::alert_cast<lt::state_update_alert>(a)) {
            for (const lt::torrent_status& st : state->status) {
                std::cout << "Progress: " << (st.progress * 100) << "%" << std::endl;
            }
        }
        else {
            // Log all alert messages for debugging
            std::cout << a->message() << std::endl;
        }
    }
}

std::string Client::getDHTStats() const {
    if (!session_) {
        return "Session not initialized";
    }
    
    session_->post_dht_stats();
    return "DHT stats requested";
}

} // namespace torrent_p2p
