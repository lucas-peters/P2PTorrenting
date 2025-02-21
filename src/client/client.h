#ifndef CLIENT_H
#define CLIENT_H

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>
#include <string>
#include <memory>
#include <map>
#include <vector>

namespace torrent_p2p {

class Client {
public:
    Client(int port = 6882);
    ~Client();

    // Connect to the DHT network using bootstrap nodes
    void connectToDHT(const std::vector<std::pair<std::string, int>>& bootstrap_nodes);
    
    // Add a torrent to download
    void addTorrent(const std::string& torrentFilePath, const std::string& savePath);
    
    // Start the client
    void start();
    
    // Stop the client
    void stop();
    
    // Get progress of a specific torrent
    double getProgress(const std::string& torrentFilePath);
    
    // Get DHT statistics
    std::string getDHTStats() const;

    // Print the status of all torrents
    void printStatus() const;

private:
    std::unique_ptr<lt::session> session_;
    std::map<std::string, lt::torrent_handle> torrents_;
    int port_;
    
    // Initialize the session with DHT settings
    void initializeSession();
    
    // Handle alerts from libtorrent
    void handleAlerts();
};

} // namespace torrent_p2p

#endif