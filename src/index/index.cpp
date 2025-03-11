# include "index.hpp"
#define NUM_REPLICATIONS 2

namespace torrent_p2p {
    Index::Index(int port, const std::string& env, const std::string& ip) : Node(port, env, ip) {
        // Call the base class start() method first to initialize messenger_
        Node::start();
        // Then call our own start() method
        start();
    }

    Index::Index(int port, const std::string& env, const std::string& ip,
        const std::string& state_file) : Node(port, env, ip, state_file) {
        // Call the base class start() method first to initialize messenger_
        Node::start();
        // Then call our own start() method
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
            [this](const lt::tcp::endpoint& sender, const IndexMessage& message) {
                this->handleIndexMessage(sender, message);
            }
        );
        std::cout << "Index Handler set successfully" << std::endl;
    } else {
        std::cerr << "ERROR: Messenger object not initialized in Index::start()" << std::endl;
    }
    
    // Find our node ID in the index_nodes_ list
    int count = 0;
    for (auto& node : index_nodes_) {
        if (ip_ == node.address().to_string()) {
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
}

std::vector<std::string> Index::generateKeywords(const std::string& title) {
    std::vector<std::string> keywords;
    std::string processedTitle = title;
    std::string currentWord;
    
    // Common words to filter out (stop words)
    static const std::unordered_set<std::string> stopWords = {
        "the", "it", "but", "on", "and", "for", "with", "this", "that", "from", "are", "was", "not",
        "but", "what", "all", "were", "when", "your", "can", "said", "there", "use",
        "each", "which", "she", "he", "his", "her", "how", "their", "will", "other", 
        "about", "out", "many", 
    };

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
        // remove empty words, short words, and words in our list
        if (currentWord.empty() || currentWord.length() < 3 || stopWords.find(currentWord) != stopWords.end()) {
            continue;
        }

        keywords.push_back(currentWord);
    }
    std::cout << "Keywords Generated: " << std::endl;
    for(auto& keyword: keywords) {
        std::cout << keyword << std::endl;
    }

    return keywords;
}

void Index::addTorrent(const std::string& title, const std::string& magnet) {
    // If we are already tracking this torrent don't bother
    std::unique_lock<std::shared_mutex> lock(data_mutex_);
    if (titleToMagnet.find(title) != titleToMagnet.end())
        return;
    
    // Store the torrent locally regardless of sharding
    // This ensures we have the magnet link when needed
    titleToMagnet[title] = magnet;

    std::vector<std::string> keywords = generateKeywords(title);
    
    // Distribute keywords to responsible nodes
    for (auto& keyword : keywords) {
        size_t keyword_node_id = getResponsibleNodeIndex(keyword, total_nodes_);
        
        // Distribute to primary and replica nodes
        for (int i = 0; i <= NUM_REPLICATIONS; i++) {
            size_t target_node_id = (keyword_node_id + i) % total_nodes_;
            
            // If this node is responsible, store locally
            if (target_node_id == id_) {
                if (keywordToTitle.find(keyword) == keywordToTitle.end()) {
                    keywordToTitle[keyword] = std::vector<std::string>{title};
                } else {
                    auto& titles = keywordToTitle[keyword];
                    if (std::find(titles.begin(), titles.end(), title) == titles.end()) {
                        titles.push_back(title);
                    }
                }
                std::cout << "Stored keyword '" << keyword << "' locally for title: " << title << std::endl;
            } 
            // Otherwise, send to the responsible node
            else {
                sendKeywordAddMessage(index_nodes_[target_node_id], keyword, title, magnet);
            }
        }
    }
}

void Index::sendKeywordAddMessage(const lt::tcp::endpoint& target, const std::string& keyword, 
                                 const std::string& title, const std::string& magnet) {
    if (!messenger_) {
        std::cerr << "Error: Messenger not initialized, can't send keyword" << std::endl;
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

void Index::handleKeywordAddMessage(const lt::tcp::endpoint& sender, const KeywordAddMessage& message) {
    std::string keyword = message.keyword();
    std::string title = message.title();
    std::string magnet = message.magnet();
    
    std::unique_lock<std::shared_mutex> lock(data_mutex_);
    
    // Store the torrent information
    if (titleToMagnet.find(title) == titleToMagnet.end()) {
        titleToMagnet[title] = magnet;
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

std::vector<std::pair<std::string, std::string>> Index::searchTorrent(const std::string& keyword) {
    std::vector<std::pair<std::string, std::string>> pairs;
    
    // Check if this node is responsible for the keyword
    size_t keyword_node_id = getResponsibleNodeIndex(keyword, total_nodes_);
    bool is_responsible = false;
    
    // Check if we're primary or replica for this keyword
    for (int i = 0; i <= NUM_REPLICATIONS; i++) {
        if (id_ == (keyword_node_id + i) % total_nodes_) {
            is_responsible = true;
            break;
        }
    }
    
    // If we're responsible, search locally
    if (is_responsible) {
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
    } 
    // Otherwise, forward the search to the responsible node
    else {
        // Create a WantMessage and send it to the responsible node
        IndexMessage message;
        WantMessage* wantMsg = message.mutable_wanttorrent();
        wantMsg->set_keyword(keyword);
        wantMsg->set_request_identifier(generateRequestId());
        
        // Send to the primary node responsible for this keyword
        messenger_->queueMessage(index_nodes_[keyword_node_id], message);
        std::cout << "Forwarded search for keyword '" << keyword << "' to responsible node" << std::endl;
        
        // Note: In a real implementation, we would wait for the response
        // For simplicity, we're returning an empty result set here
    }
    
    return pairs;
}

std::string Index::generateRequestId() {
    // Simple request ID generator
    static int counter = 0;
    return "req_" + std::to_string(++counter) + "_" + std::to_string(time(nullptr));
}

void Index::sendGiveMessage(const lt::tcp::endpoint& sender, const std::string& keyword, const std::string& request_identifier) {
    std::vector<std::pair<std::string, std::string>> pairs = searchTorrent(keyword);

    IndexMessage responseMsg;

    GiveMessage* giveMsg = responseMsg.mutable_givetorrent();
    giveMsg->set_keyword(keyword);
    giveMsg->set_request_identifier(request_identifier);

    for (auto& pair : pairs) {
        TorrentResult* resultMsg = giveMsg->add_results();
        resultMsg->set_title(pair.first);
        resultMsg->set_magnet(pair.second);
    }


    messenger_->queueMessage(sender, responseMsg);
    std::cout << "Sent: " << pairs.size() << " search results for keyword: " << keyword << std::endl;

}

void Index::handleGiveMessage(const lt::tcp::endpoint& sender, const GiveMessage& message) {
    std::string keyword = message.keyword();
    std::string request_id = message.request_identifier();
    
    std::cout << "Received search results for keyword: " << keyword << std::endl;
    
    // Process the results
    for (int i = 0; i < message.results_size(); i++) {
        const TorrentResult& result = message.results(i);
        std::cout << "Title: " << result.title() << ", Magnet: " << result.magnet() << std::endl;
        
        // Store the results locally for future reference
        std::unique_lock<std::shared_mutex> lock(data_mutex_);
        titleToMagnet[result.title()] = result.magnet();
    }
    
    // In a real implementation, we would notify the waiting client about these results
}

void Index::handleIndexMessage(const lt::tcp::endpoint& sender, const IndexMessage& message) {
    if (message.has_addtorrent()) {
        std::cout << "Received addTorrent message" << std::endl;
        const AddMessage& addMsg = message.addtorrent();
        addTorrent(addMsg.title(), addMsg.magnet());
    } else if (message.has_wanttorrent()) {
        std::cout << "Received wantTorrent message" << std::endl;
        const WantMessage& wantMsg = message.wanttorrent();
        sendGiveMessage(sender, wantMsg.keyword(), wantMsg.request_identifier());
    } else if (message.has_givetorrent()) {
        std::cout << "Received giveTorrent message" << std::endl;
        const GiveMessage& giveMsg = message.givetorrent();
        handleGiveMessage(sender, giveMsg);
    } else if (message.has_keywordadd()) {
        std::cout << "Received keywordAdd message" << std::endl;
        const KeywordAddMessage& keywordMsg = message.keywordadd();
        handleKeywordAddMessage(sender, keywordMsg);
    }
}

size_t Node::getResponsibleNodeIndex(const std::string& keyword, size_t nodeCount) {
    std::hash<std::string> hasher;
    size_t hash = hasher(keyword);
    
    return hash % nodeCount;
}

bool Node::isResponsibleForTorrent(const std::string& keyword, size_t nodeCount) {
    size_t responsibleId = getResponsibleNodeIndex(keyword, nodeCount);
    for (int i = 0; i <= NUM_REPLICATIONS; i++) {
        if (id_ == (responsibleId + i) % nodeCount)
            return true;
    }
    return false;
}

} // namespace
