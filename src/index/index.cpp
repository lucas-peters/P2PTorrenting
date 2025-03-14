# include "index.hpp"
#define NUM_REPLICATIONS 2

namespace torrent_p2p {
    Index::Index(int port, const std::string& env, const std::string& ip) : Node(port, env, ip) {
        Node::start();
        start();
    }

    Index::Index(int port, const std::string& env, const std::string& ip,
        const std::string& state_file) : Node(port, env, ip, state_file) {
        Node::start();
        start();
    }

    Index::~Index() {
        stop();
    }

    void Index::stop() {
        running_ = false;
        Node::stop();
    }

    void Index::handleAlerts() {
        if (!session_) return;
        
        std::vector<lt::alert*> alerts;
        session_->pop_alerts(&alerts);
        
        for (lt::alert* a : alerts) {
            if (auto* dht_get = lt::alert_cast<lt::dht_get_peers_alert>(a)) {
                std::cout << "DHT get peers alert: " << dht_get->info_hash << std::endl;
            }
            else if (auto* dht_announce = lt::alert_cast<lt::dht_announce_alert>(a)) {
                std::cout << "DHT announce alert: " << dht_announce->info_hash << std::endl;
            }
        }
    }

void Index::start() {
    running_ = true;

    if (messenger_) {
        messenger_->setIndexHandler(
            [this](const IndexMessage& message) {
                this->handleIndexMessage(message);
            }
        );
        std::cout << "Index Handler set successfully" << std::endl;
    } else {
        std::cerr << "ERROR: Messenger object not initialized in Index::start()" << std::endl;
    }
    
    // get our id
    int count = 0;
    for (auto& node : index_nodes_) {
        if (ip_ == node.first) {
            id_ = count;
            std::cout << "This index node has ID: " << id_ << std::endl;
            break;
        }
        count++;
    }
    
    // id_ wasn't set correctly
    if (id_ == -1) {
        std::cerr << "*** Index node IP set to an invalid value ***" << std::endl;
    }
    
    total_nodes_ = index_nodes_.size();
    std::cout << "Total index nodes: " << total_nodes_ << std::endl;

    try {
        initializeHeartbeat();
    } catch (const std::exception& e) {
        std::cerr << "[Index] Exception during Heartbeat initialization: " << e.what() << std::endl;
    }
}

std::vector<std::string> Index::generateKeywords(const std::string& title) {
    std::vector<std::string> keywords;
    std::string processedTitle = title;
    std::string currentWord;

    std::replace(processedTitle.begin(), processedTitle.end(), '_', ' ');
    std::replace(processedTitle.begin(), processedTitle.end(), '.', ' ');

    std::string spacedTitle;
    for (size_t i = 0; i < processedTitle.size(); i++) {
        char c = processedTitle[i];

        if (i < 1) {
            if(std::isupper(c)) {
                c = std::tolower(c);
            }
            spacedTitle += c;
            continue;
        }
        // remove camelcase and make all letters lower case
        if (std::isupper(c)) {
            if(std::islower(processedTitle[i - 1])) {
                spacedTitle += ' ';
            }
            c = std::tolower(c);
        }

        spacedTitle += c;
    }

    std::istringstream iss(spacedTitle);
    
    while (iss >> currentWord) {
        keywords.push_back(currentWord);
    }

    std::cout << "Keywords Generated: " << std::endl;
    for(auto& keyword: keywords) {
        std::cout << keyword << std::endl;
    }

    return keywords;
}

void Index::addTorrent(const std::string& title, const std::string& magnet) {
    std::vector<std::string> keywords;
    std::vector<std::pair<lt::tcp::endpoint, std::string>> keyword_distribution;
    bool already_tracking = false;
    
    { // Lock
        std::unique_lock<std::shared_mutex> lock(data_mutex_);
        
        // If we are already tracking this torrent don't bother
        if (titleToMagnet.find(title) != titleToMagnet.end()) {
            already_tracking = true;
        } else {
            // store the torrent locally regardless of sharding
            titleToMagnet[title] = magnet;
            keywords = generateKeywords(title);
        }
    } // Lock released

    if (already_tracking) {
        std::cout << "already tracking" << std::endl;
        return;
    }
    
    for (auto& keyword : keywords) {
        size_t keyword_node_id = getResponsibleNodeIndex(keyword);
        
        // Distribute to primary and replica nodes
        for (int i = 0; i <= NUM_REPLICATIONS; i++) {
            std::cout << "sending to index: " << (keyword_node_id + i) % total_nodes_ << std::endl;
            size_t target_node_id = (keyword_node_id + i) % total_nodes_;
            
            // If this node is responsible, add to local processing list
            if (target_node_id == id_) {
                keyword_distribution.push_back({lt::tcp::endpoint(), keyword});
            } 
            // set an endpoint for the query
            else {
                lt::tcp::endpoint target(lt::make_address_v4(index_nodes_[target_node_id].first), 
                    index_nodes_[target_node_id].second);
                keyword_distribution.push_back({target, keyword});
            }
        }
    }

    // if target is null, we handle locally, if its set send it to other indexes
    for (const auto& [target, keyword] : keyword_distribution) {
        if (target == lt::tcp::endpoint()) {
            handleKeywordAddMessage(keyword, title, magnet);
        } else {
            sendKeywordAddMessage(target, keyword, title, magnet);
        }
    }
}

std::vector<std::pair<std::string, std::string>> Index::searchTorrent(const std::string& keyword) {
    std::vector<std::pair<std::string, std::string>> pairs;

    std::shared_lock<std::shared_mutex> lock(data_mutex_);
    if (keywordToTitle.find(keyword) == keywordToTitle.end()) {
        std::cout << "No matching torrents found for keyword: " << keyword << std::endl;
        return pairs;
    }
    
    std::vector<std::string> titles = keywordToTitle[keyword];
    for (auto& title : titles) {
        std::cout << "Found torrent match: " << title << std::endl;
        pairs.push_back({title, titleToMagnet[title]});
    }
    
    return pairs;
}

void Index::sendKeywordAddMessage(const lt::tcp::endpoint& target, const std::string& keyword, 
                                 const std::string& title, const std::string& magnet) {
    if (!messenger_) {
        std::cout << "Error: Messenger not initialized, can't send keyword" << std::endl;
        return;
    }
    
    IndexMessage message;
    KeywordAddMessage* keywordMsg = message.mutable_keywordadd();
    keywordMsg->set_keyword(keyword);
    keywordMsg->set_title(title);
    keywordMsg->set_magnet(magnet);
    
    messenger_->queueMessage(target, message);
    std::cout << "Sent keyword '" << keyword << "' to node at " 
              << target.address().to_string() << ":" << target.port() << std::endl;
}

void Index::sendWantMessage(const IndexMessage& message) {
    if (!messenger_) {
        std::cout << "Error: Messenger not initialized, can't send want message" << std::endl;
        return;
    }
    if (!message.has_wanttorrent()) {
        std::cout << "Incorrect message type passed to sendWantMessage" << std::endl;
        return;
    }
    
    size_t responsibleNode = getResponsibleNodeIndex(message.wanttorrent().keyword());

    for (size_t i = 0; i <= NUM_REPLICATIONS; i++) {
        if (i == id_)
            continue;

        IndexMessage new_message = message;
        new_message.set_sender_ip(ip_);
        new_message.set_sender_port(port_);
        
        // Send to the primary node responsible for this keyword
        lt::tcp::endpoint target(lt::make_address_v4(index_nodes_[i].first), index_nodes_[i].second + 2000);
        messenger_->queueMessage(target, new_message);
        std::cout << "Forwarded search for keyword '" << message.wanttorrent().keyword() << "' to node " << i << std::endl;
    }
}

void Index::sendGiveMessage(const IndexMessage& message, const std::vector<std::pair<std::string, std::string>>& pairs) {
    if (!messenger_) {
        std::cout << "Error: Messenger not initialized, can't send give message" << std::endl;
        return;
    }
    
    IndexMessage new_message = message;
    new_message.set_sender_ip(ip_);
    new_message.set_sender_port(port_);
    GiveMessage* giveMsg = new_message.mutable_givetorrent();
    for (auto& pair: pairs) {
        TorrentResult* result = giveMsg->add_results();
        result->set_title(pair.first);
        result->set_magnet(pair.second);
    }
    
    messenger_->queueMessage(lt::tcp::endpoint(lt::make_address_v4(new_message.source_ip()), new_message.source_port()), new_message);
    std::cout << "Sent give message to node: " << new_message.source_ip() << ":" << new_message.source_port() << std::endl;
}



void Index::handleKeywordAddMessage(const std::string& keyword, const std::string& title, const std::string& magnet) {
    
    std::unique_lock<std::shared_mutex> lock(data_mutex_);
    
    // check if we are already tracking this title
    if (titleToMagnet.find(title) == titleToMagnet.end()) {
        titleToMagnet[title] = magnet;
        std::cout << "added title to mapping" << std::endl;
    }
    // Add the keyword mapping
    if (keywordToTitle.find(keyword) == keywordToTitle.end()) {
        keywordToTitle[keyword] = std::vector<std::string>{title};
    } else {
        auto& titles = keywordToTitle[keyword];
        if (std::find(titles.begin(), titles.end(), title) == titles.end()) {
            titles.push_back(title);
        }
    }
    
    std::cout << "Received and stored keyword '" << keyword << "' for title: " << title << std::endl;
}

void Index::handleIndexMessage(const IndexMessage& message) {
    // Check if we've seen this message before using message_id
    std::string cache_key;
    
    if (message.request_id().size() > 0) {
        std::cout << "Received want message with request id: " << message.request_id() << "from: " << message.source_ip() << ":" << message.source_port() << std::endl;
        cache_key = message.request_id();
    } else {
        std::cout << "Received want message without request id from: " << message.source_ip() << ":" << message.source_port() << std::endl;
        cache_key = message.source_ip() + ":" + 
                   std::to_string(message.source_port());
        
        if (message.has_wanttorrent()) {
            cache_key += ":want:" + message.wanttorrent().keyword() + std::to_string(std::time(nullptr));
        } else if (message.has_givetorrent()) {
            cache_key += ":give:" + message.givetorrent().keyword() + std::to_string(std::time(nullptr));
        }
    }
    
    bool is_new_message = false;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        is_new_message = message_cache_.insert(cache_key).second;
        
        // limit cache size to prevent memory issues
        if (message_cache_.size() > 10000) {
            message_cache_.clear(); // naive cache
        }
    }
    
    if (!is_new_message) {
        std::cout << "Ignoring duplicate message (already in cache)" << std::endl;
        return;
    }
    
    if (message.has_addtorrent()) {
        std::cout << "Received addTorrent message" << std::endl;
        addTorrent(message.addtorrent().title(), message.addtorrent().magnet());

    } else if (message.has_wanttorrent()) {
        std::cout << "Received wantTorrent message" << std::endl;
        handleWantMessage(message);

    } else if (message.has_givetorrent()) {
        std::cout << "Received giveTorrent message" << std::endl;
        handleGiveMessage(message);

    } else if (message.has_keywordadd()) {
        std::cout << "Received keywordAdd message" << std::endl;
        const KeywordAddMessage& keywordMsg = message.keywordadd();
        handleKeywordAddMessage(keywordMsg.keyword(), keywordMsg.title(), keywordMsg.magnet());
    }
}

void Index::handleWantMessage(const IndexMessage& message) {
    const std::string& keyword = message.wanttorrent().keyword();
    
    // Check if we're responsible for this keyword
    if (!isResponsibleForTorrent(keyword)) {
        std::cout << "Not responsible for keyword '" << keyword << "', forwarding" << std::endl;
        sendWantMessage(message);
        return;
    }
    std::vector<std::pair<std::string, std::string>> pairs = searchTorrent(keyword);
    
    if (pairs.empty()) {
        std::cout << "No results found for keyword '" << keyword << "'" << std::endl;
    }
    
    // Send results back (even if empty)
    sendGiveMessage(message, pairs);
}

void Index::handleGiveMessage(const IndexMessage& message) {
    if (!message.has_givetorrent()) {
        std::cerr << "Invalid message type in handleGiveMessage" << std::endl;
        return;
    }
    
    const GiveMessage& giveMsg = message.givetorrent();
    std::string keyword = giveMsg.keyword();
    std::string request_id = message.request_id();
    
    std::cout << "Received " << giveMsg.results_size() << " results for keyword '" 
              << keyword << "' (request: " << request_id << ")" << std::endl;
    
    // Process the results
    for (int i = 0; i < giveMsg.results_size(); i++) {
        const TorrentResult& result = giveMsg.results(i);
        std::cout << "Result " << (i+1) << ": " << result.title() << std::endl;
        
        // Store the result locally
        std::unique_lock<std::shared_mutex> lock(data_mutex_);
        titleToMagnet[result.title()] = result.magnet();
    }
}

// Determines which node is responsible for the keyword mapping
size_t Index::getResponsibleNodeIndex(const std::string& keyword) {
    // taking a hash of the keyword
    unsigned long hash = 5381;
    for (char c : keyword) {
        hash = ((hash << 5) + hash) + c; 
    }
    
    std::cout << "Keyword: " << keyword << " hashes to node: " << hash % total_nodes_ << std::endl;
    // mod the hash by the number of nodes to get the responsible id
    return hash % total_nodes_;
}

// Determines if we are responsible for the mapping
bool Index::isResponsibleForTorrent(const std::string& keyword) {
    size_t responsibleNode = getResponsibleNodeIndex(keyword);
    
    // Check if we are the primary node or one of the replica nodes
    for (int i = 0; i <= NUM_REPLICATIONS; i++) {
        if (id_ == (responsibleNode + i) % total_nodes_) {
            return true;
        }
    }
    return false;
}

void Index::initializeHeartbeat() {
    std::cout << "[Index] Initializing Heartbeat" << std::endl;
    if (gossip_ && !index_nodes_.empty()) {
        gossip_->setIndexHeartbeatHandler([this](const lt::tcp::endpoint& sender) {
            std::cout << "Received index heartbeat response from: " << sender.address() << ":" << sender.port() << std::endl;
            heartbeat_manager_->processHeartbeatResponse(sender);
        });
        
        // Convert index_nodes_ from std::vector<std::pair<std::string, int>> to std::vector<lt::tcp::endpoint>
        std::vector<lt::tcp::endpoint> endpoint_nodes;
        for (const auto& node : index_nodes_) {
            try {
                lt::tcp::endpoint endpoint(lt::make_address_v4(node.first), node.second + 1000);
                endpoint_nodes.push_back(endpoint);
                std::cout << "[Index] Added endpoint: " << node.first << ":" << node.second << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[Index] Error creating endpoint for " << node.first << ":" << node.second 
                          << " - " << e.what() << std::endl;
            }
        }
        
        // Create heartbeat manager
        try {
            heartbeat_manager_ = std::make_unique<IndexHeartbeat>(*gossip_, endpoint_nodes, ip_, port_ + 1000);
            std::cout << "[Index] Heartbeat manager created successfully" << std::endl;
            heartbeat_manager_->start();
            std::cout << "[Index] Heartbeat started successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Index] Error creating heartbeat manager: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "[Index] Cannot initialize heartbeat - gossip not initialized or no index nodes" << std::endl;
    }
}

} // namespace
