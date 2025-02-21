#include "bootstrap_node.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

using namespace torrent_p2p;

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 6881;
    BootstrapNode node(port);
    node.start();
    
    // Keep the program running
    while (true) {
        std::cout << node.getDHTStats() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    return 0;
}