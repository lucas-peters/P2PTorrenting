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
        | lt::alert::dht_notification
        | lt::alert::port_mapping_notification);
    
    // Enable DHT
    pack.set_bool(lt::settings_pack::enable_dht, true);
    
    // Only listen on localhost
    pack.set_str(lt::settings_pack::listen_interfaces, "127.0.0.1:" + std::to_string(port_));
    
    // Disable external connections
    pack.set_bool(lt::settings_pack::enable_upnp, false);
    pack.set_bool(lt::settings_pack::enable_natpmp, false);
    pack.set_bool(lt::settings_pack::enable_lsd, false);
    pack.set_bool(lt::settings_pack::enable_outgoing_utp, false);
    pack.set_bool(lt::settings_pack::enable_incoming_utp, false);
    
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
        } else if (auto* state = lt::alert_cast<lt::state_update_alert>(a)) {
            for (const lt::torrent_status& st : state->status) {
                std::cout << "Progress: " << (st.progress * 100) << "%" << std::endl;
            }
        } else if (auto* peers_alert = lt::alert_cast<lt::dht_get_peers_reply_alert>(a)) {
            std::cout << "\nFound peers for info hash: " << peers_alert->info_hash << std::endl;
            std::cout << "Number of peers: " << peers_alert->num_peers() << std::endl;
        } else {
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

void Client::printStatus() const {
    if (!session_) {
        return;
    }
    
    if (torrents_.empty()) {
        std::cout << "No active torrents" << std::endl;
        return;
    }

    std::cout << "Active torrents:" << std::endl;
    for (const auto& [path, handle] : torrents_) {
        std::cout << "Torent: " << path << std::endl;
        std::cout << "\tProgress: " << (handle.status().progress * 100) << "%" << std::endl;
        std::cout << "\tDownload Rate: " << handle.status().download_rate << std::endl;
        std::cout << "\tUpload Rate: " << handle.status().upload_rate << std::endl;
        std::cout << "\tState: " << handle.status().state << std::endl;
    }
    std::cout << std::endl;
}

void Client::searchDHT(const std::string& infoHash) {
    if (!session_) {
        std::cout << "Session not initialized" << std::endl;
        return;
    }
    lt::sha1_hash targetHash;
    if(infoHash.size() == 40) {
        // Hash in hex format
        from_hex(infoHash.c_str(), targetHash.data());
    } else if (infoHash.size() == 20) {
        // Hash in binary format
        std::copy(infoHash.begin(), infoHash.end(), targetHash.begin());    
    } else {
        std::cout << "Invalid info hash" << std::endl;
        return;
    }

    session_->dht_get_peers(targetHash);
}

} // namespace torrent_p2p
