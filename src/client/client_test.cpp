#include "client.h"
#include <fstream>
#include <iostream>
#include <cassert> // For assertions
#include <thread>
#include <chrono>
#include <filesystem> // For file existence checks (C++17)
#include <libtorrent/alert_types.hpp> // for add_torrent_alert and torrent_finished_alert

namespace fs = std::filesystem;

// Helper function to create a dummy torrent file
void createDummyTorrentFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    file << content; // Simplified content - doesn't need to be a real torrent
    file.close();
}
//Helper function to create an empty file
void createEmptyFile(const std::string& filename) {
    std::ofstream file(filename);
    file.close();
}

// Helper function to wait for a specific alert
template <typename AlertType>
bool waitForAlert(lt::session& session, int timeout_seconds = 10) {
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(timeout_seconds)) {
        std::vector<lt::alert*> alerts;
        session.pop_alerts(&alerts);
        for (lt::alert* alert : alerts) {
            if (lt::alert_cast<AlertType>(alert)) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

void testAddTorrent() {
    // --- Setup ---
    int client_port = 6885; // Use a different port for testing
    torrent_p2p::Client client(client_port);
    client.start(); //starts client
    std::string save_path = "test_downloads";
    fs::create_directory(save_path); // Create the download directory

    // --- Test 1: Valid .torrent File ---
    createDummyTorrentFile("test.torrent", "d8:announce30:http://example.com/announce4:infod6:lengthi123e4:name4:test4:pathl10:testfile.txte7:piece20:abcdefghij1234567890ee"); //simplified, but structrually valid
    client.addTorrent("test.torrent", save_path);

    // Wait for the add_torrent_alert
    bool alert_received = waitForAlert<lt::add_torrent_alert>(*client.session_); //Access the session directly
    assert(alert_received);

    // Check if the torrent is in the session
    // (Assuming you have a way to get torrent handles or info-hashes from your client)
    // Get the info-hash.  libtorrent might change the announce URL, so we can't rely on that
    lt::torrent_info ti("test.torrent");
    lt::sha1_hash info_hash = ti.info_hash();
    assert(client.hasTorrent(info_hash));

    // Wait for a short time (let it try to connect to peers, etc.)
     std::this_thread::sleep_for(std::chrono::seconds(2));

    //Check status is not error
    lt::torrent_status status = client.torrents_[info_hash].status();
    assert(status.state != lt::torrent_status::error);

    // --- Test 2: Invalid .torrent File ---
    createEmptyFile("invalid.torrent"); // Or create a file with invalid bencode
    client.addTorrent("invalid.torrent", save_path);
	bool error_alert_received = waitForAlert<lt::add_torrent_alert>(*client.session_);
    assert(error_alert_received);

    // --- Test 3: Existing Torrent ---
    client.addTorrent("test.torrent", save_path); // Add the same torrent again
    //You should check your logging output.  The client should print "Torrent already exists"

    // --- Test 4: Save Path Verification (Basic) ---
    // We can't *fully* test downloading here without a tracker/peer, but we can check
    // if the directory structure is created.
    // Wait a bit more for potential file creation
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // Check for directory creation (assuming your torrent has files in a subdirectory)
    // This check depends on the structure of your test.torrent file!
    assert(fs::exists(save_path + "/test")); // Change "test" if needed
    assert(fs::exists(save_path + "/test/testfile.txt")); // Change "test" if needed

    // --- Test 5: No Session ---
    torrent_p2p::Client client2(client_port+1); //Dont start the session
    client2.addTorrent("test.torrent", save_path);
    //Check there isn't an alert
    bool no_session_alert_received = waitForAlert<lt::add_torrent_alert>(*client2.session_);
    assert(!no_session_alert_received); //We expect an alert to NOT be thrown

    // --- Cleanup ---
    client.stop();
    fs::remove_all(save_path); // Clean up the download directory
    fs::remove("test.torrent");
    fs::remove("invalid.torrent");
}

int main() {
    testAddTorrent();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}