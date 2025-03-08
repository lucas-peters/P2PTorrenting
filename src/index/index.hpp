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
#include "messenger.hpp"
#include "index.pb.h"

namespace torrent_p2p {

class Index : public Node {

public:
    Index(int port = 6882);
    Index(int port, const std::string& state_file);
    ~Index();

    void addTorrent(const std::string& title, const std::string& magnet);
    std::vector<std::string> generateKeywords(const std::string& title);
    std::vector<std::pair<std::string, std::string>> searchTorrent(const std::string& keyword);

private:
    void start() override;
    void stop() override;
    // Handle DHT alerts
    void handleAlerts() override;
    void handleIndexMessage(const lt::tcp::endpoint& sender, const IndexMessage& message);
    void sendGiveMessage(const lt::tcp::endpoint& sender, const std::string& keyword, const std::string& request_id);

    std::unordered_map<std::string, std::string> titleToMagnet;
    std::unordered_map<std::string, std::vector<std::string>> keywordToTitle;

    mutable std::shared_mutex data_mutex_;

    // bloom filtering to allow for faster searching on other nodes. Instead of waiting for messages
    //BloomFilter bloom_filter_;
    //std::vector<std::pair<lt::tcp::endpoint, Bloomfilter> other_blooms;
};

}// namespace