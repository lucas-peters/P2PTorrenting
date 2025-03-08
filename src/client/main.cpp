#include "client.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <atomic>
#include <thread>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <map>
#include <functional>

using namespace torrent_p2p;
namespace fs = std::filesystem;

// // Global flag for graceful shutdown
// volatile sig_atomic_t running = 1;

// // Signal handler
// void signal_handler(int signal) {
//     std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
//     running = 0;
// }

// Function to display help information
void displayHelp() {
    std::cout << "\n===== P2P Torrenting Client - Command Reference =====\n";
    std::cout << "  add <torrent_file> <save_path>    - Add a torrent file to download\n";
    std::cout << "  magnet <info_hash> <save_path>    - Add a torrent via magnet link\n";
    std::cout << "  generate <file_path>              - Generate a torrent file from a file/directory\n";
    std::cout << "  status                            - Display status of all torrents\n";
    std::cout << "  search <info_hash>                - Search DHT network for peers with specific info hash\n";
    std::cout << "  connect <ip> <port>               - Connect to a specific bootstrap node\n";
    std::cout << "  dht                               - Display DHT statistics\n";
    std::cout << "  gossip                            - Send a test gossip message\n";
    std::cout << "  save <state_file>                 - Save DHT state to file\n";
    std::cout << "  load <state_file>                 - Load DHT state from file\n";
    std::cout << "  clear                             - Clear the console\n";
    std::cout << "  help                              - Display this help message\n";
    std::cout << "  quit                              - Exit the application\n";
    std::cout << "====================================================\n";
}

int main(int argc, char* argv[]) {
    // std::signal(SIGINT, signal_handler);
    // std::signal(SIGTERM, signal_handler);
    
    // Parse command line arguments
    int port = 6882;
    std::string state_file = "";
    bool load_state = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" || arg == "-p") {
            if (i + 1 < argc) {
                try {
                    port = std::stoi(argv[i + 1]);
                    i++;
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing port number: " << e.what() << std::endl;
                    return 1;
                }
            }
        } else if (arg == "--state" || arg == "-s") {
            if (i + 1 < argc) {
                state_file = argv[i + 1];
                load_state = true;
                i++;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --port, -p <port>    Specify port number (default: 6882)\n";
            std::cout << "  --state, -s <file>   Load DHT state from file\n";
            std::cout << "  --help, -h           Display this help message\n";
            return 0;
        }
    }

    std::cout << "Starting client on port: " << port << std::endl;

    // Initialize client
    std::unique_ptr<Client> client;
    
    if (load_state && !state_file.empty()) {
        std::cout << "Loading DHT state from: " << state_file << std::endl;
        client = std::make_unique<Client>(port, state_file);
    } else {
        client = std::make_unique<Client>(port);
    }
    
    // Display welcome message and commands
    std::cout << "\n===== P2P Torrenting Client =====\n";
    std::cout << "Client running on port: " << port << "\n";
    std::cout << "Type 'help' for available commands\n";
    std::cout << "================================\n\n";

    // Starting a background thread that prints DHT stats
    std::atomic<bool> running{true};
    std::thread stats_thread([&client, &running]() {
        while (running) {
            // Only print stats every 30 seconds to avoid cluttering the console
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (running) {
                std::cout << "\n[DHT Stats Update]\n" << client->getDHTStats() << std::endl;
                std::cout << "> ";
                std::cout.flush(); // Make sure the prompt is displayed after stats
            }
        }
    });

    // Command processing loop
    std::string input;
    while (running) {
        //std::cout << "> ";
        std::cout.flush();  // Make sure the prompt is displayed
        std::getline(std::cin, input);

        std::istringstream iss(input);
        std::string command;
        iss >> command;

        if (command.empty()) {
            continue;
        }
        
        // Convert command to lowercase for case-insensitive matching
        std::transform(command.begin(), command.end(), command.begin(), 
                      [](unsigned char c){ return std::tolower(c); });

        if (command == "quit" || command == "exit") {
            std::cout << "Shutting down client...\n";
            running = false;
            break;
        } else if (command == "help") {
            displayHelp();
        } else if (command == "clear" || command == "cls") {
            // Clear screen - works on most terminals
            std::cout << "\033[2J\033[1;1H";
        } else if (command == "add") {
            std::string torrent_file, save_path;
            if (iss >> torrent_file >> save_path) {
                try {
                    std::cout << "Adding torrent: " << torrent_file << "\n";
                    std::cout << "Save path: " << save_path << "\n";
                    client->addTorrent(torrent_file, save_path);
                } catch (const std::exception& e) {
                    std::cerr << "Error adding torrent: " << e.what() << "\n";
                }
            } else {
                std::cout << "Usage: add <torrent_file> <save_path>\n";
            }
        } else if (command == "magnet") {
            std::string info_hash, save_path;
            if (iss >> info_hash >> save_path) {
                try {
                    std::cout << "Adding magnet link with info hash: " << info_hash << "\n";
                    std::cout << "Save path: " << save_path << "\n";
                    client->addMagnet(info_hash, save_path);
                } catch (const std::exception& e) {
                    std::cerr << "Error adding magnet link: " << e.what() << "\n";
                }
            } else {
                std::cout << "Usage: magnet <info_hash> <save_path>\n";
            }
        } else if (command == "status") {
            client->printStatus();
        } else if (command == "generate") {
            std::string file_path;
            if (iss >> file_path) {
                try {
                    std::cout << "Generating torrent file for: " << file_path << "\n";
                    client->generateTorrentFile(file_path);
                    std::cout << "Torrent file generated successfully.\n";
                } catch (const std::exception& e) {
                    std::cerr << "Error generating torrent file: " << e.what() << "\n";
                }
            } else {
                std::cout << "Usage: generate <file_path>\n";
            }
        } else if (command == "search") {
            std::string info_hash;
            if (iss >> info_hash) {
                try {
                    std::cout << "Searching DHT for info hash: " << info_hash << "\n";
                    client->searchDHT(info_hash);
                } catch (const std::exception& e) {
                    std::cerr << "Error searching DHT: " << e.what() << "\n";
                }
            } else {
                std::cout << "Usage: search <info_hash>\n";
            }
        } 
        // else if (command == "connect") {
        //     std::string ip;
        //     int node_port;
        //     if (iss >> ip >> node_port) {
        //         try {
        //             std::vector<std::pair<std::string, int>> bootstrap_nodes = {
        //                 {ip, node_port}
        //             };
        //             std::cout << "Connecting to bootstrap node: " << ip << ":" << node_port << "\n";
        //             client->connectToDHT(bootstrap_nodes);
        //         } catch (const std::exception& e) {
        //             std::cerr << "Error connecting to bootstrap node: " << e.what() << "\n";
        //         }
        //     } else {
        //         std::cout << "Usage: connect <ip> <port>\n";
        //     }
        // } 
        else if (command == "dht") {
            std::cout << client->getDHTStats() << std::endl;
        } else if (command == "gossip") {
            std::cout << "Sending gossip message...\n";
            GossipMessage message;
            message.set_source_ip("127.0.0.1");
            message.set_source_port(port);
            lt::tcp::endpoint exclude(lt::make_address_v4("127.0.0.1"), port);
            client->gossip_->spreadMessage(message, exclude);
        } else if (command == "save") {
            std::string save_file;
            if (iss >> save_file) {
                try {
                    std::cout << "Saving DHT state to: " << save_file << "\n";
                    if (client->saveDHTState(save_file)) {
                        std::cout << "DHT state saved successfully.\n";
                    } else {
                        std::cerr << "Failed to save DHT state.\n";
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error saving DHT state: " << e.what() << "\n";
                }
            } else {
                std::cout << "Usage: save <state_file>\n";
            }
        } else if (command == "load") {
            std::string load_file;
            if (iss >> load_file) {
                try {
                    std::cout << "Loading DHT state from: " << load_file << "\n";
                    if (client->loadDHTState(load_file)) {
                        std::cout << "DHT state loaded successfully.\n";
                    } else {
                        std::cerr << "Failed to load DHT state.\n";
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error loading DHT state: " << e.what() << "\n";
                }
            } else {
                std::cout << "Usage: load <state_file>\n";
            }
        } else {
            std::cout << "Unknown command: " << command << "\n";
            std::cout << "Type 'help' for available commands\n";
        }
    }

    // Clean up
    if (stats_thread.joinable()) {
        stats_thread.join();
    }

    std::cout << "Client shutdown complete.\n";
    return 0;
}
