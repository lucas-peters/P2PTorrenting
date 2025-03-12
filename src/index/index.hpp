#include <algorithm>
#include <sstream>
#include <cctype>
#include <iostream>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include "node/node.hpp"
#include "gossip/index_heartbeat.hpp"
#include "messenger.hpp"
#include "index.pb.h"

namespace torrent_p2p {

// Forward declaration
class IndexHeartbeat;

class Index : public Node {

public:
    Index(int port = 6883, const std::string& env = "aws", const std::string& ip = "None");
    Index(int port, const std::string& env, const std::string& ip, const std::string& state_file);
    ~Index();

    void addTorrent(const std::string& title, const std::string& magnet);
    std::vector<std::string> generateKeywords(const std::string& title);
    std::vector<std::pair<std::string, std::string>> searchTorrent(const std::string& keyword);

private:
    int id_; // Node's id used for index sharding
    size_t total_nodes_;
    void start() override;
    void stop() override;
    // Handle DHT alerts
    void handleAlerts() override;
    void handleIndexMessage(const lt::tcp::endpoint& sender, const IndexMessage& message);
    void sendGiveMessage(const lt::tcp::endpoint& sender, const std::string& keyword, const std::string& request_id);
    void handleKeywordAddMessage(const lt::tcp::endpoint& sender, const KeywordAddMessage& message);
    void sendKeywordAddMessage(const lt::tcp::endpoint& target, const std::string& keyword, 
                               const std::string& title, const std::string& magnet);
    std::string generateRequestId();
    void handleGiveMessage(const lt::tcp::endpoint& sender, const GiveMessage& message);

    std::unordered_map<std::string, std::string> titleToMagnet;
    std::unordered_map<std::string, std::vector<std::string>> keywordToTitle;
    // For tracking pending search requests
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> pendingSearchResults;

    mutable std::shared_mutex data_mutex_;

    size_t getResponsibleNodeIndex(const std::string& keyword, size_t nodeCount);
    bool isResponsibleForTorrent(const std::string& keyword, size_t nodeCount);

    void initializeHeartbeat();
    
    // Heartbeat manager
    std::unique_ptr<IndexHeartbeat> heartbeat_manager_;

    // bloom filtering to allow for faster searching on other nodes. Instead of waiting for messages
    //BloomFilter bloom_filter_;
    //std::vector<std::pair<lt::tcp::endpoint, Bloomfilter> other_blooms;
};

}// namespace
