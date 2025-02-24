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
    void addTorrent(const std::string& torrentFile, const std::string& savePath);

    // Check if a torrent is already added
    bool hasTorrent(const lt::sha1_hash& hash) const;

    // turns a normal file into a torrent file
    void generateTorrentFile(const std::string& savePath);
    
    // Start the client
    void start();
    
    // Stop the client
    void stop();
    
    // Get progress of a specific torrent
   //double getProgress(const std::string& torrentFilePath);
    
    // Get DHT statistics
    std::string getDHTStats() const;

    // Print the status of all torrents
    void printStatus() const;

    // Seach DHT network for peers with a specific info hash
    void searchDHT(const std::string& infoHash);

    // Generate a magnet URI for a specific torrent
    std::string createMagnetURI(const std::string& torrentFilePath) const;

    // Add a magnet link to the client
    void addMagnet(const std::string& infoHash, const std::string& savePath);

    // Begins seeding a torrent file for other clients to download from
    void startSeeding(const std::string& torrentFilePath);

    // Stops seeding a torrent file for other clients to download from
    void stopSeeding(const std::string& torrentFilePath);

    // Converts a string representation of an info hash to a sha1_hash
    lt::sha1_hash stringToHash(const std::string& infoHashString) const;

private:
    std::unique_ptr<lt::session> session_;
    std::map<lt::sha1_hash, lt::torrent_handle> torrents_;
    int port_;
    bool running_;
    
    // Initialize the session with DHT settings
    void initializeSession();
    
    // Handle alerts from libtorrent
    void handleAlerts();
};

} // namespace torrent_p2p

#endif