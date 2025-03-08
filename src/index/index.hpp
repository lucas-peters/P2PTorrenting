#include <algorithm>
#include <sstream>
#include <cctype>
#include <iostream>

#include "node/node.hpp"

namespace torrent_p2p {
class Index : public Node {
public:
    Index(int port = 6882);
    Index(int port, const std::string& state_file);
    ~Index();

    void addTorrent();
    std::vector<std::string> generateKeywords();
    std::vector<std::pair<std::string, std::string>> searchTorrents(const std::string& keyword);

private:
    void start() override;
    void stop() override;
    // Handle DHT alerts
    void handleAlerts() override;
    std::unordered_map<std::string, std::string> titleToMagnet;
    // keywords can hash to multiple titles
    std::unordered_map<std::string, std::vector<std::string>> keywordToTitle;
    std::unique_ptr<Messenger> messenger_;
    int messenger_port_;

    // bloom filtering to allow for faster searching on other nodes. Instead of waiting for messages
    //BloomFilter bloom_filter_;
    //std::vector<std::pair<lt::tcp::endpoint, Bloomfilter> other_blooms;
};   
}