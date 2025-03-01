#ifndef CLIENT_H
#define CLIENT_H

#include "node/node.hpp"

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/alert_types.hpp>
#include <string>
#include <memory>
#include <map>
#include <vector>

namespace torrent_p2p {

class Client : public Node {
public:
    Client(int port = 6882);
    Client(int port, const std::string& state_file);
    ~Client();

    // Connect to the DHT network using bootstrap nodes
    void connectToDHT(const std::vector<std::pair<std::string, int>>& bootstrap_nodes);
    
    // Add a torrent to download
    void addTorrent(const std::string& torrentFilePath, const std::string& savePath);

    // Check if a torrent is already added
    bool hasTorrent(const lt::sha1_hash& hash) const;

    // turns a normal file into a torrent file
    void generateTorrentFile(const std::string& savePath);
    
    // Get progress of a specific torrent
   //double getProgress(const std::string& torrentFilePath);

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

    // save/load the client's DHT state to/from a file
    bool saveDHTState(const std::string& state_file) const;
    bool loadDHTState(const std::string& state_file);

private:
    std::map<lt::sha1_hash, lt::torrent_handle> torrents_;
    // Start the client
    void start() override;
    
    // Stop the client
    void stop() override;
    // Initialize the session with DHT settings
    void initializeSession();
    // Handle alerts from libtorrent
    void handleAlerts() override;
};

} // namespace torrent_p2p

#endif