#include "node/node.hpp"

namespace torrent_p2p {
class Index : public Node {
public:
    Index(int port = 6882);
    Index(int port, const std::string& state_file);
    ~Index();

    addTorrent();
private:
    void start() override;
    void stop() override;
    // Handle DHT alerts
    void handleAlerts() override;
};   
}