#ifndef BOOTSTRAP_NODES_H
#define BOOTSTRAP_NODES_H

#include <vector>
#include <string>

namespace torrent_p2p {

struct BootstrapNode {
    std::string ip;
    int port;
};

class BootstrapNodeManager {
public:
    BootstrapNodeManager();
    ~BootstrapNodeManager();

    std::vector<BootstrapNode> getBootstrapNodes();
    void addBootstrapNode(const std::string& ip, int port);
    void removeBootstrapNode(const std::string& ip, int port);

private:
    std::vector<BootstrapNode> bootstrapNodes_;
};

} // namespace torrent_p2p

#endif