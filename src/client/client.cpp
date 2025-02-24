#include "client.h"
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/address.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

#define BYTES_STRING_LENGTH 20
#define HEX_STRING_LENGTH 40

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
        | lt::alert::port_mapping_notification
        | lt::alert::dht_log_notification);
    
    // Enable DHT with local-only settings
    pack.set_bool(lt::settings_pack::enable_dht, true);
    pack.set_str(lt::settings_pack::dht_bootstrap_nodes, "");  // Disable external bootstrap nodes
    pack.set_int(lt::settings_pack::dht_announce_interval, 5);
    pack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
    pack.set_bool(lt::settings_pack::enable_incoming_utp, true);
    pack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
    pack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
    
    // Disable IP restrictions for local testing
    pack.set_bool(lt::settings_pack::dht_restrict_routing_ips, false);
    pack.set_bool(lt::settings_pack::dht_restrict_search_ips, false);
    pack.set_int(lt::settings_pack::dht_max_peers_reply, 100);
    pack.set_bool(lt::settings_pack::dht_ignore_dark_internet, false);
    pack.set_int(lt::settings_pack::dht_max_fail_count, 100);  // More forgiving of failures
    
    // Only listen on localhost
    pack.set_str(lt::settings_pack::listen_interfaces, "127.0.0.1:" + std::to_string(port_));
    
    session_ = std::make_unique<lt::session>(pack);
}

void Client::connectToDHT(const std::vector<std::pair<std::string, int>>& bootstrap_nodes) {
    if (!session_) {
        std::cout << "Session not initialized" << std::endl;
        return;
    }

    // Add bootstrap nodes to routing table
    for (const auto& node : bootstrap_nodes) {
        std::cout << "[Client:" << port_ << "] Adding bootstrap node: " << node.first << ":" << node.second << std::endl;
        session_->add_dht_node(std::make_pair(node.first, node.second));
        
        try {
            // Force immediate DHT lookup to this node
            lt::udp::endpoint ep(lt::make_address_v4(node.first), node.second);
            session_->dht_direct_request(ep, lt::entry{}, lt::client_data_t{});
            
            // Also announce ourselves with a generated hash
            lt::sha1_hash hash;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> dis;
            for (int i = 0; i < 5; i++) {
                reinterpret_cast<uint32_t*>(hash.data())[i] = dis(gen);
            }
            session_->dht_announce(hash, port_);
        } catch (const std::exception& e) {
            std::cerr << "[Client:" << port_ << "] Error connecting to node: " << e.what() << std::endl;
        }
    }
    
    // Start periodic DHT lookups
    std::thread([this]() {
        while (running_) {
            if (session_) {
                session_->dht_get_peers(lt::sha1_hash());
                session_->post_dht_stats();
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();
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
        running_ = false;
        session_.reset();
    }
}

bool Client::hasTorrent(const lt::sha1_hash& hash) const {
    return torrents_.find(hash) != torrents_.end();
}

void Client::addTorrent(const std::string& torrentFile, const std::string& savePath) {
    try {
        lt::add_torrent_params params;
        params.ti = std::make_shared<lt::torrent_info>(torrentFile);
        params.save_path = savePath;
        
        // Check if we already have this torrent
        lt::sha1_hash hash = params.ti->info_hash();
        if (hasTorrent(hash)) {
            std::cout << "Torrent already exists" << std::endl;
            return;
        }
        
        lt::torrent_handle handle = session_->add_torrent(params);
        torrents_[hash] = handle;
        
        std::cout << "Added torrent: " << params.ti->name() << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error adding torrent: " << e.what() << std::endl;
    }
}

void Client::addMagnet(const std::string& infoHashString, const std::string& savePath) {
    if (!session_) {
        std::cout << "Session not initialized" << std::endl;
        return;
    }

    try {
        lt::sha1_hash hash = stringToHash(infoHashString);
        
        // Check if we already have this torrent
        if (hasTorrent(hash)) {
            std::cout << "Torrent already exists" << std::endl;
            return;
        }
        
        // Create magnet URI from info hash
        std::string magnetUri = "magnet:?xt=urn:btih:" + hash.to_string();
        
        lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri);
        params.save_path = savePath;
        
        lt::torrent_handle handle = session_->add_torrent(params);
        torrents_[hash] = handle;
        
        std::cout << "Added magnet link with info hash: " << hash.to_string() << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
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
    for (const auto& [hash, handle] : torrents_) {
        auto status = handle.status();
        std::cout << "Torrent: " << hash.to_string() << std::endl;
        std::cout << "\tProgress: " << (status.progress * 100) << "%" << std::endl;
        std::cout << "\tDownload Rate: " << status.download_rate << std::endl;
        std::cout << "\tUpload Rate: " << status.upload_rate << std::endl;
        std::cout << "\tState: " << status.state << std::endl;
    }
    std::cout << std::endl;
}

void Client::handleAlerts() {
    if (!session_) return;

    std::vector<lt::alert*> alerts;
    session_->pop_alerts(&alerts);

    for (lt::alert* a : alerts) {
        if (auto* dht_stats = lt::alert_cast<lt::dht_stats_alert>(a)) {
            int total_nodes = 0;
            for (auto const& t : dht_stats->routing_table) {
                total_nodes += t.num_nodes;
            }
            std::cout << "\n[Client:" << port_ << "] DHT nodes in routing table: " << total_nodes << std::endl;
            
            // Print detailed routing table info
            if (total_nodes > 0) {
                std::cout << "[Client:" << port_ << "] Routing table details:" << std::endl;
                for (size_t i = 0; i < dht_stats->routing_table.size(); ++i) {
                    const auto& bucket = dht_stats->routing_table[i];
                    if (bucket.num_nodes > 0) {
                        std::cout << "  Bucket " << i << ": " << bucket.num_nodes << " nodes" << std::endl;
                    }
                }
            }
        } else if (auto* dht_log = lt::alert_cast<lt::dht_log_alert>(a)) {
            // Log all DHT activity for debugging
            std::cout << "[Client:" << port_ << "] DHT: " << dht_log->message() << std::endl;
        } else if (auto* state = lt::alert_cast<lt::state_update_alert>(a)) {
            for (const lt::torrent_status& st : state->status) {
                std::cout << "[Client:" << port_ << "] Progress: " << (st.progress * 100) << "%" << std::endl;
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

// dht will communicate back through alerts
void Client::searchDHT(const std::string& infoHashString) {
    if (!session_) {
        std::cout << "Session not initialized" << std::endl;
        return;
    }

    lt::sha1_hash targetHash;
    targetHash = stringToHash(infoHashString);
    session_->dht_get_peers(targetHash);
}

std::string Client::createMagnetURI(const std::string& torrentFilePath) const {
    if (!session_) {
        std::cout << "Session not initialized" << std::endl;
        return NULL;
    }
    lt::torrent_info ti(torrentFilePath);
    std::string magnetURI = lt::make_magnet_uri(ti);
    std::cout << "Magnet URI: " << magnetURI << std::endl;
    return magnetURI;
}

lt::sha1_hash Client::stringToHash(const std::string& infoHashString) const {
    lt::sha1_hash hash;

    if (infoHashString.size() == HEX_STRING_LENGTH) {
        // Hexadecimal string
        lt::aux::from_hex(infoHashString, hash.data());
    } else if (infoHashString.size() == BYTES_STRING_LENGTH) {
        // Binary string
        std::array<char, BYTES_STRING_LENGTH> hashArray;
        std::copy(infoHashString.begin(), infoHashString.end(), hashArray.begin());
        hash = lt::sha1_hash(hashArray);
    } else {
        std::cout << "Invalid info hash string size" << std::endl;
    }

    return hash;
}

} // namespace torrent_p2p
