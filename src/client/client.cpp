#include "client.hpp"

#define BYTES_STRING_LENGTH 20
#define HEX_STRING_LENGTH 40
#define PIECE_LENGTH 16 * 1024

namespace torrent_p2p {

Client::Client(int port) : Node(port) {
    start();
}

Client::Client(int port, const std::string& state_file) : Node(port, state_file) {
    start();
    // The base class constructor already loads the DHT state, so we don't need to call loadDHTState again
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
        | lt::alert::dht_log_notification
        | lt::alert::piece_progress_notification
        | lt::alert::storage_notification);

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
        session_->add_dht_node(std::make_pair(node.first, node.second)); //adding the boostrap_node to the DHT
        // pings to bootrap_node (in the list), if successfull adds to DHT
        
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
    
    // Start periodic DHT lookups //like a heartbeat
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

// libtorrent add_torrent adds the torrent to the session
// if the torrent is file is missing or incomplete it tries to download from peers
// if the torrent is complete it starts seeding
// We can bypass this if we want to roll our own torrent protocol but it will be a pain in the ass
void Client::addTorrent(const std::string& torrentFilePath, const std::string& savePath) {
    try {
        // load the torrent parameters from file
        lt::add_torrent_params params = lt::load_torrent_file(torrentFilePath);
        params.save_path = savePath;
        std::cout << "params.save_path: " << params.save_path << std::endl;

        // this loop strips the absolute file path added by load_torrent_file, so thats in the correct format
        for (lt::file_index_t i(0); i < params.ti->files().end_file(); ++i) {
            std::string file_path = params.ti->files().file_path(i);
            
            // Get just the filename without any path
            std::string filename = file_path;
            size_t last_slash = file_path.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                filename = file_path.substr(last_slash + 1);
                std::cout << "filename: " << filename << std::endl;
                // Rename the file to just its basename
                params.ti->rename_file(i, filename);
            }
        }

        // Print torrent file information
        std::cout << "Torrent info:" << std::endl;
        std::cout << "Total size: " << params.ti->total_size() << " bytes" << std::endl;
        std::cout << "Piece length: " << params.ti->piece_length() << " bytes" << std::endl;
        std::cout << "Number of pieces: " << params.ti->num_pieces() << std::endl;
        std::cout << "Number of files: " << params.ti->num_files() << std::endl;
        
        std::cout << "\nFiles in torrent:" << std::endl;
        for (int i = 0; i < params.ti->num_files(); ++i) {
            std::cout << "File " << i << ": " << params.ti->files().file_path(i) 
                     << " (Size: " << params.ti->files().file_size(i) << " bytes)" << std::endl;
            
            // Check if file exists at the expected location
            std::string full_path = savePath + "/" + params.ti->files().file_path(i);
            std::error_code ec;
            if (std::filesystem::exists(full_path, ec)) {
                auto file_size = std::filesystem::file_size(full_path, ec);
                std::cout << "Found file at: " << full_path << std::endl;
                std::cout << "Actual file size: " << file_size << " bytes" << std::endl;
            } else {
                std::cout << "File not found at: " << full_path << std::endl;
            }
        }

        // Check if we already have this torrent
        lt::sha1_hash hash = params.ti->info_hash();
        if (hasTorrent(hash)) {
            std::cout << "Torrent already exists" << std::endl;
            return;
        }
        
        lt::torrent_handle handle = session_->add_torrent(params);
        torrents_[hash] = handle;
        
        std::cout << "Added torrent: " << params.ti->name() << std::endl;
        
        // Print detailed initial torrent status
        lt::torrent_status status = handle.status();
        std::cout << "\nInitial torrent status:" << std::endl;
        std::cout << "State: " << 
            (status.is_seeding ? "seeding" : 
             status.is_finished ? "finished" : 
             "downloading") << std::endl;
        std::cout << "Progress: " << (status.progress * 100) << "%" << std::endl;
        std::cout << "Total done: " << status.total_done << " bytes" << std::endl;
        std::cout << "Total wanted: " << status.total_wanted << " bytes" << std::endl;
        std::cout << "State: " << status.state << std::endl;
        
        // Check piece verification state
        std::cout << "\nPiece verification:" << std::endl;
        std::cout << "Checking files: " << (status.state == lt::torrent_status::checking_files) << std::endl;
        std::cout << "Checking resume data: " << (status.state == lt::torrent_status::checking_resume_data) << std::endl;
        
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

// libtorrent dht communicates through sending asynchronous alerts
void Client::handleAlerts() {
    if (!session_) return;

    std::vector<lt::alert*> alerts;
    session_->pop_alerts(&alerts);

    for (lt::alert const* a : alerts) {
        if (auto* st = lt::alert_cast<lt::state_changed_alert>(a)) {
            if (st->state == lt::torrent_status::checking_files) {
                if (st->handle.torrent_file())
                    std::cout << "Started verifying pieces for torrent: " << st->handle.torrent_file()->name() << std::endl;
                else
                    std::cout << "Started verifying pieces for torrent" << std::endl;
            }
        }
        else if (auto* pf = lt::alert_cast<lt::piece_finished_alert>(a)) {
            // Calculate verification progress
            lt::torrent_status status = pf->handle.status();
            float progress = status.progress * 100;
            std::cout << "\rVerifying pieces: " << static_cast<int>(progress) << "% complete" << std::flush;
        }
        else if (auto* sc = lt::alert_cast<lt::storage_moved_alert>(a)) {
            std::cout << "\nTorrent data moved to: " << sc->storage_path() << std::endl;
        }
        else if (auto* fa = lt::alert_cast<lt::file_completed_alert>(a)) {
            if (fa->handle.torrent_file())
                std::cout << "\nFile completed in torrent: " << fa->handle.torrent_file()->name() << std::endl;
            else
                std::cout << "\nFile completed in torrent" << std::endl;
        }
        else if (auto* fea = lt::alert_cast<lt::fastresume_rejected_alert>(a)) {
            std::cout << "Fast resume data rejected: " << fea->message() << std::endl;
        }
        // Handle verification completion
        else if (auto* tc = lt::alert_cast<lt::torrent_checked_alert>(a)) {
            if (tc->handle.torrent_file())
                std::cout << "\nPiece verification completed for torrent: " << tc->handle.torrent_file()->name() << std::endl;
            else
                std::cout << "\nPiece verification completed for torrent" << std::endl;
            
            lt::torrent_status status = tc->handle.status();
            if (status.is_seeding) {
                std::cout << "All pieces verified successfully - torrent is complete" << std::endl;
            } else {
                std::cout << "Some pieces failed verification - torrent is incomplete" << std::endl;
                std::cout << "Progress: " << (status.progress * 100) << "%" << std::endl;
            }
        }
        else if (auto* dht_stats = lt::alert_cast<lt::dht_stats_alert>(a)) {
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
        }
        // } else if (auto* dht_log = lt::alert_cast<lt::dht_log_alert>(a)) {
        //     // Log all DHT activity for debugging
        //     std::cout << "[Client:" << port_ << "] DHT: " << dht_log->message() << std::endl;
        // } else if (auto* state = lt::alert_cast<lt::state_update_alert>(a)) {
        //     for (const lt::torrent_status& st : state->status) {
        //         std::cout << "[Client:" << port_ << "] Progress: " << (st.progress * 100) << "%" << std::endl;
        //     }
        // } else if (auto* peers_alert = lt::alert_cast<lt::dht_get_peers_reply_alert>(a)) {
        //     std::cout << "\nFound peers for info hash: " << peers_alert->info_hash << std::endl;
        //     std::cout << "Number of peers: " << peers_alert->num_peers() << std::endl;
        // } else if (auto* pa = lt::alert_cast<lt::peer_connect_alert>(a)) {
        //     // We are ensuring that all peers are using ChaCha20 encryption
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
        // } else {
        //     // Log all alert messages for debugging
        //     std::cout << a->message() << std::endl;
        // }
    }
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

void Client::generateTorrentFile(const std::string& savePath) {
    // output location, might change this
    try {
        // find last . to change extension to .torrent
        int pos = savePath.find_last_of(".");
        std::string torrentFilePath = savePath.substr(0, pos) + ".torrent";
        // need to get file size for libtorrents file system
        std::filesystem::path filePath(savePath);
        int fileSize = std::filesystem::file_size(filePath);

        // adding file to libtorrent file_storage, and using that object to create a torrent
        lt::file_storage fs;
        fs.add_file(savePath, fileSize);
        std::cout << "added file to fs" << std::endl;
        lt::create_torrent torrent(fs, PIECE_LENGTH);
        std::cout << "created torrent" << std::endl;
        std::cout << "Generating hashes..." << std::endl;

        // Open the file *here*, just before generating hashes.  This guarantees
        // it's open in the correct mode and won't be interfered with.
        std::ifstream inputFile(savePath, std::ios::binary);
        if (!inputFile) {
            throw std::runtime_error("Could not open input file for hashing: " + savePath);
        }

        // Set the file to be read when generating hashes
        lt::set_piece_hashes(torrent, ".");

        inputFile.close(); // Close the file *after* set_piece_hashes.
        std::cout << "Hashes generated." << std::endl;
        lt::entry ent = torrent.generate();
        std::cout << "generated torrent" << std::endl;

        std::ofstream out(torrentFilePath, std::ios::binary);
        if (!out) {
          throw std::runtime_error("Could not open torrent output file:" + torrentFilePath);
        }
        lt::bencode(std::ostream_iterator<char>(out), ent);
        out.close();

    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

}

// checks the torrent handle is valid
// libtorrent sets seeding to true automatically
// sets the seed flag to false, doesn't let other clients download from it
// void Client::stopSeeding(lt::torrent_handle& handle) {
// }

// void Client::startSeeding(lt::torrent_handle& handle) {
// }

} // namespace bracket
