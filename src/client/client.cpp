#include "client.hpp"

#define BYTES_STRING_LENGTH 20
#define HEX_STRING_LENGTH 40
#define PIECE_LENGTH 16 * 1024

namespace torrent_p2p {

Client::Client(int port, const std::string& env, const std::string& ip) : Node(port, env, ip) {
    setSavePaths(env);
    start();
}

Client::Client(int port, const std::string& env, const std::string& ip, 
    const std::string& state_file) : Node(port, env, ip, state_file) {
    setSavePaths(env);
    start();
    // The base class constructor already loads the DHT state, so we don't need to call loadDHTState again
}

Client::~Client() {
    stop();
}

void Client::setSavePaths(const std::string& env) {
    if (env == "docker" || env == "aws") {
        torrent_path_ = "/app/torrents/";
        download_path_ = "/app/downloads/";
    }
}

void Client::start() {
    Node::start();
    
    // Start handling alerts in a separate thread
    alert_thread_ = std::make_unique<std::thread>([this]() {
        while (session_) {
            handleAlerts();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // peer cache thread
    peer_cache_thread_ = std::make_unique<std::thread>([this]() {
        while (session_) {
            updatePeerCache();
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    });

    // Ensure gossip_ is initialized before setting the handler
    if (gossip_) {
        std::cout << "Setting reputation handler for Gossip..." << std::endl;
        gossip_->setReputationHandler(
            [this](const ReputationMessage& message, const lt::tcp::endpoint& sender) {
                this->handleReputationMessage(message, sender);
            }
        );
        std::cout << "Reputation handler set successfully" << std::endl;
    } else {
        std::cerr << "ERROR: Gossip object not initialized in Client::start()" << std::endl;
    }

    if (messenger_) {
        messenger_->setIndexHandler(
            [this](const IndexMessage& message) {
                this->handleIndexMessage(message);
            }
        );
        std::cout << "Index Handler set successfully" << std::endl;
    } else {
        std::cerr << "ERROR: Messenger object not initialized in Client::start()" << std::endl;
    }

    std::cout << "Client started on port " << port_ << std::endl;
}

void Client::stop() {
    if (session_) {
        // DHT will be automatically stopped when session is destroyed
        running_ = false;
        
        // join threads
        if (alert_thread_ && alert_thread_->joinable()) {
            alert_thread_->join();
        }
        if (peer_cache_thread_ && peer_cache_thread_->joinable()) {
            peer_cache_thread_->join();
        }
        
        // Stop gossip and messenger
        if (gossip_) {
            gossip_->stop();
        }
        // if (messenger_) {
        //     messenger_->stop();
        // }
        
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
// Make sure that torrentFilePath is only the local path to the torrent, it is automatically appended to "/app/torrents/" for our docker images
void Client::addTorrent(const std::string& torrentFilePath) {
    try {
        // Load the torrent file into a byte vector
        std::ifstream torrentFileStream(torrent_path_ + torrentFilePath, std::ios_base::binary);
        std::vector<char> torrentFileBytes((std::istreambuf_iterator<char>(torrentFileStream)),
                                            std::istreambuf_iterator<char>());

        // Create torrent_info from the byte vector
        lt::torrent_info torrentInfo(torrentFileBytes.data(), torrentFileBytes.size());

        // Create add_torrent_params from torrent_info
        lt::add_torrent_params params;
        params.ti = std::make_shared<lt::torrent_info>(torrentInfo);
        std::cout << "params.ti->name(): " << params.ti->name() << std::endl;
        params.save_path = download_path_;
        std::cout << "params.save_path: " << params.save_path << std::endl;

        // this loop strips the absolute file path added by load_torrent_file so thats in the correct format
        for (lt::file_index_t i(0); i < params.ti->files().end_file(); ++i) {
            std::string file_path = params.ti->files().file_path(i);
            std::cout << file_path << std::endl;
            
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
            std::string full_path = download_path_ + params.ti->files().file_path(i);
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


        //std::cout << "Adding to index title: " << title << std::endl;
        std::string magnetLink = lt::make_magnet_uri(*(params.ti));
        addIndex(params.ti->name(), magnetLink);
        
    } catch (const std::exception& e) {
        std::cout << "Error adding torrent: " << e.what() << std::endl;
    }
}

void Client::addMagnet(const std::string& magnet) {
    if (!session_) {
        std::cout << "Session not initialized" << std::endl;
        return;
    }

    try {
        lt::sha1_hash hash = stringToHash(magnet);
        
        // Check if we already have this torrent
        if (hasTorrent(hash)) {
            std::cout << "Torrent already exists in session" << std::endl;
            return;
        }
        
        // Create magnet URI from info hash
        std::string magnetUri = "magnet:?xt=urn:btih:" + hash.to_string();
        
        lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri);
        params.save_path = download_path_;
        
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
        else if (auto* nodes_alert = lt::alert_cast<lt::dht_live_nodes_alert>(a)) {
            std::cout << "\n[Client:" << port_ << "] DHT live nodes" << std::endl;
            std::cout << "Number of nodes: " << nodes_alert->num_nodes() << std::endl;
            
            // Add these nodes to our peer cache
            // std::vector<std::pair<lt::sha1_hash, lt::tcp::endpoint>> nodes = nodes_alert->nodes();
            for (auto & node: nodes_alert->nodes()) {
                
            // Add to peer cache
            // addPeerToCache(lt::tcp::endpoint(node);
            
            // Optionally, print some info about the node
            std::cout  << "ip: " << node.second.address().to_string() << "port: " << node.second.port() 
                    << ", id: " << nodes_alert->node_id.to_string() << "..." << std::endl;
            }
        }
        /* PIECE TRACKING */
        else if (auto* bfa = lt::alert_cast<lt::block_finished_alert>(a)) {
            std::cout << "finished downloading block from peer: " << bfa->endpoint << std::endl;
            lt::sha1_hash info_hash = bfa->handle.info_hash();
            
            // mutex unlocks when this variabale goes out of scope at the end of the else if block
            std::lock_guard<std::mutex> lock(torrent_tracker_mutex_);
            // if a tracker doesn't exist for the torrent instantiate one
            if(torrent_trackers_.find(info_hash) == torrent_trackers_.end()) {
                torrent_trackers_[info_hash] = std::make_unique<PieceTracker>();
            }
            torrent_trackers_[info_hash]->add_block(bfa->piece_index, bfa->block_index, bfa->endpoint);
            
        }
        else if (auto* hfa = lt::alert_cast<lt::hash_failed_alert>(a)) {
            std::cout << "Hash check failed! getting bad peers" << std::endl;
            lt::sha1_hash info_hash = hfa->handle.info_hash();

            if(torrent_trackers_.find(info_hash) != torrent_trackers_.end()) {
                torrent_trackers_[info_hash]->record_bad_piece(hfa->piece_index);

                // printing bad piece information for debugging
                auto bad_peers = torrent_trackers_[info_hash]->get_piece_contributors(hfa->piece_index);
                for (auto& peer: bad_peers) {
                    std::cout << "Bad piece contribution from peer: " << peer 
                            << " for torrent: " << info_hash << std::endl;
                }
            }
        }
        else if (auto* pfa = lt::alert_cast<lt::piece_finished_alert>(a)) {
            std::cout << "piece finished downloading" << std::endl;
            lt::sha1_hash info_hash = pfa->handle.info_hash();
            if(torrent_trackers_.find(info_hash) != torrent_trackers_.end()) {
                torrent_trackers_[info_hash]->record_good_piece(pfa->piece_index);
            }
        }
        else if (auto* at = lt::alert_cast<lt::add_torrent_alert>(a)) {
            // When a torrent is added, start the download timer
            lt::sha1_hash info_hash = at->handle.info_hash();
            if (!at->handle.status().is_seeding) {
                startDownloadTimer(info_hash);
            }
        }
        else if (auto * tfa = lt::alert_cast<lt::torrent_finished_alert>(a)) {
            // Get the info hash directly - works with both older and newer libtorrent versions
            lt::sha1_hash info_hash = tfa->handle.info_hash();
            
            if (torrent_trackers_.find(info_hash) != torrent_trackers_.end()) {
                auto updates = torrent_trackers_[info_hash]->get_reputation_updates();
                
                std::cout << "Torrent " << info_hash << " complete. Sending reputation updates for " 
                        << updates.size() << " peers" << std::endl;
                
                sendGossip(updates);
                
                // Remove tracker for this torrent
                torrent_trackers_.erase(info_hash);
            }
            reportDownloadCompletion(info_hash);
        }
    }

    // gets download status for torrents actively downloading
    std::lock_guard<std::mutex> lock(progress_mutex_);
    for (const auto& [hash, handle] : torrents_) {
        if (!handle.is_valid()) continue;
        
        auto status = handle.status();
        if (status.state == lt::torrent_status::downloading) {
            auto it = download_progress_.find(hash);
            if (it != download_progress_.end()) {
                auto& progress = it->second;
                auto now = std::chrono::steady_clock::now();
                auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - progress.last_update_time).count() / 1000.0;
                
                if (time_diff >= .1) {  // Update every 100ms
                    progress.last_progress = progress.current_progress;
                    progress.current_progress = status.progress;
                    
                    // Calculate speed (in KB/s)
                    double progress_diff = progress.current_progress - progress.last_progress;
                    double size_diff = progress_diff * status.total_wanted;
                    double speed = size_diff / time_diff / 1024.0;
                    
                    // Update average speed with some smoothing
                    if (progress.avg_speed_kBs == 0) {
                        progress.avg_speed_kBs = speed;
                    } else {
                        progress.avg_speed_kBs = progress.avg_speed_kBs * 0.7 + speed * 0.3;
                    }
                    
                    // Calculate ETA
                    double remaining_bytes = (1.0 - status.progress) * status.total_wanted;
                    if (progress.avg_speed_kBs > 0) {
                        progress.est_time_remaining_sec = remaining_bytes / (progress.avg_speed_kBs * 1024.0);
                    }
                    
                    progress.last_update_time = now;
                }
            }
        }
    }
    // log the heck of out it
    //printStatus();

        
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

// create a sha-1 hash from magnet string
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

void Client::generateTorrentFile(const std::string& fileName) {
    // output location, might change this
    try {
        // find last . to change extension to .torrent
        int pos = fileName.find_last_of(".");
        std::string rawName = fileName.substr(0, pos);
        std::string torrentFilePath = torrent_path_ + rawName + ".torrent";

        // need to get file size for libtorrents file system
        std::filesystem::path filePath(download_path_ + fileName);
        int fileSize = std::filesystem::file_size(filePath);

        // adding file to libtorrent file_storage, and using that object to create a torrent
        lt::file_storage fs;
        fs.add_file(fileName, fileSize);
        lt::create_torrent torrent(fs, PIECE_LENGTH);

        std::cout << "Generating hashes..." << std::endl;
        lt::set_piece_hashes(torrent, download_path_);
        std::cout << "Hashes generated." << std::endl;

        lt::entry ent = torrent.generate();

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

void Client::sendGossip(std::vector<std::pair<lt::tcp::endpoint, int>> peer_reputation) const{
    if (!gossip_) {
        std::cerr << "ERROR: Cannot send gossip - gossip_ is null" << std::endl;
        return;
    }
    
    if (peer_reputation.empty()) {
        std::cout << "No gossip to send - peer_reputation vector is empty" << std::endl;
        return;
    }
    
    std::cout << "Sending gossip for " << peer_reputation.size() << " peers" << std::endl;
    lt::tcp::endpoint empty_endpoint(lt::address_v4::any(), 0);
    
    for (auto& peer : peer_reputation) {
        std::cout << "Sending gossip about peer " << peer.first.address().to_string() 
                  << ":" << peer.first.port() << " with reputation " << peer.second << std::endl;
        try {
            auto message = gossip_->createGossipMessage(peer.first, peer.second);
            std::cout << "Created gossip message" << std::endl;
            
            gossip_->spreadMessage(message, empty_endpoint);
            std::cout << "Gossip message sent successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception while sending gossip: " << e.what() << std::endl;
        }
    }
}

// Handler for incoming reputation messages
void Client::handleReputationMessage(const ReputationMessage& message, const lt::tcp::endpoint& sender) {
    // Extract information from the message
    std::string subject_ip = message.subject_ip();
    int subject_port = message.subject_port();
    int reputation_delta = message.reputation();
    
    std::cout << "Received reputation update from " << sender.address().to_string() 
              << ":" << sender.port() << " about peer " << subject_ip << ":" 
              << subject_port << " with delta " << reputation_delta << std::endl;
    
    // Update our local peer cache with this reputation information
    lt::tcp::endpoint peer_key(lt::make_address_v4(subject_ip), subject_port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    if (peer_cache_.find(peer_key) != peer_cache_.end()) {
        // Update existing peer
        peer_cache_[peer_key] += reputation_delta;
        std::cout << "Updated peer " << peer_key << " reputation to " 
                  << peer_cache_[peer_key] << std::endl;
        if (peer_cache_[peer_key] < 0) {
            banNode(peer_key);
        }
    }
}

void Client::handleIndexMessage(const IndexMessage& message) {
    // TODO
    if (message.has_givetorrent()) {
        std::cout << "Search Results: " << std::endl;
        const GiveMessage& giveMsg = message.givetorrent();
        for (auto& result : giveMsg.results()) {
            std::cout << "Title: " << result.title() << " Magnet Link: " << result.magnet() << std::endl;
        }
    }
}

void Client::addIndex(const std::string& title, const std::string& magnet) {
    if (!messenger_) {
        std::cerr << "Client Messenger not initialized, can't add index" << std::endl;
        return;
    }
    try {
        // naive approach just send to first index in our list
        auto& node = index_nodes_[0];
        lt::tcp::endpoint endpoint(lt::make_address_v4(node.first), node.second + 2000);

        IndexMessage message;
        message.set_request_id(generateRequestId());

        AddMessage* addMsg = message.mutable_addtorrent();
        addMsg->set_title(title);
        addMsg->set_magnet(magnet);

        messenger_->queueMessage(endpoint, message);

    } catch (const std::exception& e) {
        std::cerr << "Error adding Index: " << e.what() << std::endl;
    }
}

void Client::searchIndex(const std::string& keyword) {
    if (!messenger_) {
        std::cerr << "Client Messenger not initialized, cant search indexes" << std::endl;
        return;
    }
    try {
        // naive approach just send to first index in our list
        auto& node = index_nodes_[0];
        lt::tcp::endpoint endpoint(lt::make_address_v4(node.first), node.second + 2000);
        IndexMessage message;
        message.set_request_id(generateRequestId());

        WantMessage* wantMsg = message.mutable_wanttorrent();
        std::cout << "Sending search messeage to endpoint: " << endpoint.address() << ":" << endpoint.port() << std::endl;
        wantMsg->set_keyword(keyword);
        messenger_->queueMessage(endpoint, message);
        
    } catch (const std::exception& e) {
        std::cerr << "Error searching index: " << e.what() << std::endl;
    }
}

void Client::updatePeerCache() {
    auto peers = session_->session_state().dht_state.nodes;
    std::lock_guard<std::mutex> lock(peers_mutex_);

    for (const auto& peer : peers) {
        // converting udp object to tcp
        lt::tcp::endpoint temp_peer(peer.address(), peer.port());
        
        // checking cache
        if (peer_cache_.find(temp_peer) != peer_cache_.end()) {
            peer_cache_[temp_peer] = 100;
        }
    }
}

// Ban a node from the DHT network
// This is the only way to ban nodes, wont work for localhost tests since it will just ban all nodes
void Client::banNode(const lt::tcp::endpoint& endpoint) {
    if (!session_) {
        std::cerr << "Cannot ban node: session not initialized" << std::endl;
        return;
    }
    
    try {
        std::cout << "Banning node " << endpoint.address().to_string() 
                  << ":" << endpoint.port() << std::endl;
        
        // Create an IP filter
        lt::ip_filter filter = session_->get_ip_filter();
        
        // Add the node's IP to the filter
        // This blocks all connections from this IP address
        filter.add_rule(endpoint.address(), endpoint.address(), lt::ip_filter::blocked);
        
        // Apply the updated filter to the session
        session_->set_ip_filter(filter);
        
        std::cout << "Added node " << endpoint.address().to_string() 
                  << " to IP filter" << std::endl;
        
        // Also remove the node from our peer cache to avoid future interactions
        std::lock_guard<std::mutex> lock(peers_mutex_);
        if (peer_cache_.find(endpoint) != peer_cache_.end()) {
            peer_cache_.erase(endpoint);
            std::cout << "Removed node from peer cache" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error banning node: " << e.what() << std::endl;
    }
}

void Client::corruptAllTorrents(double corruption_percent) {
    for (auto& torrent : torrents_) {
        corruptTorrentData(torrent.first, corruption_percent);
    }
}

void Client::corruptTorrentData(const lt::sha1_hash& info_hash, double corruption_percent) {
    std::cout << "Attempting to corrupt torrent data for testing..." << std::endl;
    auto it = torrents_.find(info_hash);
    if (it == torrents_.end()) {
        std::cerr << "Torrent not found with hash: " << info_hash.to_string() << std::endl;
        return;
    }
    
    lt::torrent_handle handle = it->second;
    if (!handle.is_valid()) {
        std::cerr << "Invalid torrent handle" << std::endl;
        return;
    }
    
    // get torrent info hash from the torrent file
    std::shared_ptr<const lt::torrent_info> ti = handle.torrent_file();
    if (!ti) {
        std::cerr << "Missing torrent info" << std::endl;
        return;
    }
    
    std::cout << "Corrupting files for torrent: " << ti->name() << std::endl;
    
    // pausing suspends other nodes ability to download from it, libtorrent treats it as not in the session
    handle.pause();
    std::cout << "Torrent paused" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // corrupt each file in the torrent, for our case we should only have single files
    for (lt::file_index_t i(0); i < ti->files().end_file(); ++i) {
        std::string file_path = download_path_ + ti->files().file_path(i);
        std::cout << "Processing file: " << file_path << std::endl;
        
        if (!std::filesystem::exists(file_path)) {
            std::cerr << "File not found: " << file_path << std::endl;
            continue;
        }
        std::streamsize file_size = std::filesystem::file_size(file_path);
        if (file_size < 1024) {
            std::cout << "File too small to corrupt: " << file_path << std::endl;
            continue;
        }
        std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!file) {
            std::cerr << "Cannot open file: " << file_path << std::endl;
            continue;
        }
        
        // Calculate bytes to corrupt (at least 100 bytes)
        size_t bytes_to_corrupt = std::max<size_t>(static_cast<size_t>(file_size * corruption_percent / 100.0), 100);
        
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<std::streamsize> dist(0, file_size - 1);
        
        std::cout << "Corrupting " << bytes_to_corrupt << " bytes in file: " << file_path << std::endl;

        for (size_t j = 0; j < bytes_to_corrupt; ++j) {
            // jump to a random position and read a byte
            std::streampos pos = dist(rng);
            file.seekg(pos);
            char byte;
            file.read(&byte, 1);
            
            // flip the bits in the byte
            byte = ~byte;

            file.seekp(pos);
            file.write(&byte, 1);
        }
        
        file.close();
        std::cout << "File corrupted: " << file_path << std::endl;
    }
    
    // when seed mode is enabled libtorrent won't verify local hashes
    std::cout << "Setting seed mode flag to skip hash checking..." << std::endl;
    handle.set_flags(lt::torrent_flags::seed_mode);
    
    // torrent is active again
    handle.resume();
    std::cout << "Torrent resumed with corrupted data" << std::endl;

    std::cout << "Torrent data corruption complete. When peers download from this client, "
              << "they should detect bad pieces and trigger hash_failed_alert events." << std::endl;
}

void Client::startDownloadTimer(const lt::sha1_hash& info_hash) {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    download_start_times_[info_hash] = std::chrono::steady_clock::now();
    
    DownloadProgress progress;
    progress.last_update_time = std::chrono::steady_clock::now();
    download_progress_[info_hash] = progress;
    
    std::cout << "Started download timer for torrent: " << info_hash.to_string() << std::endl;
}

void Client::reportDownloadCompletion(const lt::sha1_hash& info_hash) {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    auto it = download_start_times_.find(info_hash);
    if (it != download_start_times_.end()) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - it->second).count();
        
        std::string torrent_name = "Unknown";
        auto handle_it = torrents_.find(info_hash);
        if (handle_it != torrents_.end() && handle_it->second.torrent_file()) {
            torrent_name = handle_it->second.torrent_file()->name();
        }
        
        std::cout << "\n===== Download Completed =====\n";
        std::cout << "Torrent: " << torrent_name << std::endl;
        std::cout << "Info Hash: " << info_hash.to_string() << std::endl;
        
        // Format duration
        int hours = static_cast<int>(duration / 3600);
        int minutes = static_cast<int>((duration - hours * 3600) / 60);
        int seconds = static_cast<int>(duration % 60);
        
        std::cout << "Total download time: ";
        if (hours > 0) {
            std::cout << hours << "h " << minutes << "m " << seconds << "s";
        } else if (minutes > 0) {
            std::cout << minutes << "m " << seconds << "s";
        } else {
            std::cout << seconds << "s";
        }
        std::cout << std::endl;
        
        // Get total downloaded size
        if (handle_it != torrents_.end()) {
            auto status = handle_it->second.status();
            std::cout << "Total size: " << formatSize(status.total_wanted) << std::endl;
            std::cout << "Average speed: " << std::fixed << std::setprecision(2) 
                      << (status.total_wanted / static_cast<double>(duration) / 1024.0) << " KB/s" << std::endl;
        }
        
        std::cout << "=============================\n";
        
        // Clean up
        download_start_times_.erase(it);
        download_progress_.erase(info_hash);
    }
}

// Helper method to format file sizes
std::string Client::formatSize(std::int64_t bytes) const {
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int suffix_idx = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && suffix_idx < 4) {
        size /= 1024;
        suffix_idx++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << suffixes[suffix_idx];
    return ss.str();
}

// checks the torrent handle is valid
// libtorrent sets seeding to true automatically
// sets the seed flag to false, doesn't let other clients download from it
// void Client::stopSeeding(lt::torrent_handle& handle) {
// }

// void Client::startSeeding(lt::torrent_handle& handle) {
// }

} // namespace bracket
