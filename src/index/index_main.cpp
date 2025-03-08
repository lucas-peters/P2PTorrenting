#include "index.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <csignal>

using namespace torrent_p2p;

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

// Signal handler
void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = 0;
}

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 6883;

    std::cout << " Starting index node on port " << port << std::endl;

    try {
        Index node(port);
        // Spin the loop, so Node doesn't die
        while(running) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error starting bootstrap node: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}