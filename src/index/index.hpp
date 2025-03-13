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
    void handleIndexMessage(const IndexMessage& message);
    void sendWantMessage(const IndexMessage& message);
    void sendGiveMessage(const IndexMessage& message, const std::vector<std::pair<std::string, std::string>>& pairs);
    void handleWantMessage(const IndexMessage& message);
    void handleGiveMessage(const IndexMessage& message);
    void handleKeywordAddMessage(const std::string& keyword, const std::string& title, const std::string& magnet);
    void sendKeywordAddMessage(const lt::tcp::endpoint& target, const std::string& keyword,
                                const std::string& title, const std::string& magnet);

    std::unordered_map<std::string, std::string> titleToMagnet;
    std::unordered_map<std::string, std::vector<std::string>> keywordToTitle;
    // For tracking pending search requests
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> pendingSearchResults;

    mutable std::shared_mutex data_mutex_;

    size_t getResponsibleNodeIndex(const std::string& keyword);
    bool isResponsibleForTorrent(const std::string& keyword);

    void initializeHeartbeat();

    // message cache
    std::unordered_set<std::string> message_cache_;
    std::mutex cache_mutex_;

    // Heartbeat manager
    std::unique_ptr<IndexHeartbeat> heartbeat_manager_;
};

}// namespace
