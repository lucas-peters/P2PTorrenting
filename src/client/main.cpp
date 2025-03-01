#include "client.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <atomic>
#include <thread>

using namespace torrent_p2p;

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 6882;
    std::cout << "port: " << port << std::endl;

    // Run as client
    Client client(port);
    
    // Connect to bootstrap nodes
    std::vector<std::pair<std::string, int>> bootstrap_nodes = {
        {"127.0.0.1", 6881}  // local bootstrap node
    };
    
    client.connectToDHT(bootstrap_nodes);
    
    std::cout << "Client running. Commands available:" << std::endl;
    std::cout << "  add <torrent_file> <save_path>" << std::endl;
    std::cout << "  status" << std::endl;
    std::cout << "  quit" << std::endl;

    // Starting a background thread that prints DHT stats
    std::atomic<bool> running{true};
    std::thread stats_thread([&client, &running]() {
        while (running) {
            std::cout << client.getDHTStats() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    });

    std::string input;
    // Keep the program running
    while (running) {
        std::cout << "> ";
        std::getline(std::cin, input);

        std::istringstream iss(input);
        std::string command;
        iss >> command;

        if (command == "quit") {
            running = false;
            break;
        } else if (command == "add") {
            std::string torrent_file, save_path;
            if (iss >> torrent_file >> save_path) {
                std::cout << "save_path in main: " << save_path << std::endl;
                client.addTorrent(torrent_file, save_path);
            } else {
                std::cout << "Usage: add <torrent_file> <save_path>" << std::endl;
    
            }
        
        } else if (command == "status") {
            client.printStatus();
        } else if (command == "generate") {
            std::string save_path("/Users/lucaspeters/Documents/GitHub/P2PTorrenting/6882/DOOM1.wad");
            client.generateTorrentFile(save_path);
        } else if (command == "create") {
            std::string save_path("/Users/lucaspeters/Documents/GitHub/P2PTorrenting/6882/DOOM1.torrent");
            client.createMagnetURI(save_path);
        }
        else if (!command.empty()) {
            std::cout << "Unknown command. Available commands:" << std::endl;
            std::cout << "  add <torrent_file> <save_path>" << std::endl;
            std::cout << "  status" << std::endl;
            std::cout << "  quit" << std::endl;
        }
    }

    return 0;
}
