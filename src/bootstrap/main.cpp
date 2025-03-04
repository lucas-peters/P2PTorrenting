#include "bootstrap_node.hpp"
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
    // Register signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    int port = (argc > 1) ? std::stoi(argv[1]) : 6881;
    
    std::cout << "Starting bootstrap node on port " << port << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    
    // Print the local IP addresses to help with configuration
    std::cout << "Available on:" << std::endl;
    std::cout << "  - 127.0.0.1:" << port << " (localhost)" << std::endl;
    std::cout << "  - 0.0.0.0:" << port << " (all interfaces)" << std::endl;
    
    try {
        BootstrapNode node(port);
        
        // Keep the program running until signal is received
        while (running) {
            std::string stats = node.getDHTStats();
            std::cout << "=== DHT Stats ===" << std::endl;
            std::cout << stats << std::endl;
            std::cout << "=================" << std::endl;
            
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        
        std::cout << "Shutting down bootstrap node..." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error starting bootstrap node: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error starting bootstrap node" << std::endl;
        return 1;
    }
    
    return 0;
}