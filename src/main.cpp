#include "bootstrap_node.h"
#include "client.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace torrent_p2p;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [bootstrap|client] [port]" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    int port = (argc > 2) ? std::stoi(argv[2]) : 6881;

    if (mode == "bootstrap") {
        // Run as bootstrap node
        BootstrapNode node(port);
        node.start();
        
        std::cout << "Bootstrap node running. Press Ctrl+C to exit." << std::endl;
        
        // Keep the program running
        while (true) {
            std::cout << node.getDHTStats() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    else if (mode == "client") {
        // Run as client
        Client client(port);
        
        // Connect to bootstrap nodes
        std::vector<std::pair<std::string, int>> bootstrap_nodes = {
            {"127.0.0.1", 6881}  // Connect to local bootstrap node
        };
        
        client.connectToDHT(bootstrap_nodes);
        client.start();
        
        std::cout << "Client running. Press Ctrl+C to exit." << std::endl;
        
        // Keep the program running
        while (true) {
            std::cout << client.getDHTStats() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    else {
        std::cerr << "Invalid mode. Use 'bootstrap' or 'client'" << std::endl;
        return 1;
    }

    return 0;
}
