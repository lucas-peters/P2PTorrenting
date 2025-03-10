# include "index.hpp"

namespace torrent_p2p {
    Index::Index(int port, const std::string& env) : Node(port, env) {
        // Call the base class start() method first to initialize messenger_
        Node::start();
        // Then call our own start() method
        start();
    }

    Index::Index(int port, const std::string& env, const std::string& state_file) : Node(port, env, state_file) {
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
    
    titleToMagnet[title] = magnet;

    std::vector<std::string> keywords;

    keywords = generateKeywords(title);
    for (auto& keyword : keywords) {
        if (keywordToTitle.find(keyword) == keywordToTitle.end()) {
            keywordToTitle[keyword] = std::vector<std::string> {title};
            continue;
        }
        auto& titles = keywordToTitle[keyword];
        if (std::find(titles.begin(), titles.end(), title) == titles.end()) {
            titles.push_back(title);
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
        

    std::vector<std::string> titles;

    titles = keywordToTitle[keyword];
    for (auto & title : titles) {
        std::cout << "Found torrent match: " << title << std::endl;
        pairs.push_back({title, titleToMagnet[title]});
    }
    return pairs;
}

void Index::sendGiveMessage(const lt::tcp::endpoint& sender, const std::string& keyword, const std::string& request_identifier) {
    std::vector<std::pair<std::string, std::string>> pairs = this->searchTorrent(keyword);

    IndexMessage responseMsg;

    GiveMessage* giveMsg = responseMsg.mutable_givetorrent();
    giveMsg->set_keyword(keyword);
    giveMsg->set_request_identifier(request_identifier);

    for (auto& pair : pairs) {
        TorrentResult* resultMsg = giveMsg->add_results();
        resultMsg->set_title(pair.first);
        resultMsg->set_magnet(pair.second);
    }

    if (messenger_) {
        messenger_->queueMessage(sender, responseMsg);
        std::cout << "Sent: " << pairs.size() << "search results for keyword: " << keyword << std::endl;
    } else {
        std::cerr << "Error: Messenger not intialized, can't send search results" << std::endl;
    }
}

void Index::handleIndexMessage(const lt::tcp::endpoint& sender, const IndexMessage& message) {
    if (message.has_addtorrent()) {
        std::cout << "Recieved addTorrent message" << std::endl;
        const AddMessage& addMsg = message.addtorrent();
        addTorrent(addMsg.title(), addMsg.magnet());
    } else if (message.has_wanttorrent()) {
        std::cout << "Received wantTorrent mesesage" << std::endl;
        const WantMessage& wantMsg = message.wanttorrent();
        sendGiveMessage(sender, wantMsg.keyword(), wantMsg.request_identifier());
    }
}

} // namespace