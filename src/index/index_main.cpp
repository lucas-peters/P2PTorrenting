#include "index.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <csignal>
#include <string>

using namespace torrent_p2p;

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

// Signal handler
void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = 0;
}

void displayHelp(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --port, -p <port>    Specify port number (default: 6883)\n";
    std::cout << "  --env, -e <env>      Specify environment (default: lucas)\n";
    std::cout << "                       Valid environments: lucas, docker, aws, shanaya\n";
    std::cout << "  --help, -h           Display this help message\n";
}

int main(int argc, char* argv[]) {
    // Register signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Default values
    int port = 6883;
    std::string env = "lucas";
    
    // Parse command line arguments
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
            } else {
                std::cerr << "Missing port number after " << arg << std::endl;
                return 1;
            }
        } else if (arg == "--env" || arg == "-e") {
            if (i + 1 < argc) {
                env = argv[i + 1];
                i++;
                // Validate environment
                if (env != "lucas" && env != "docker" && env != "aws" && env != "shanaya") {
                    std::cerr << "Invalid environment: " << env << std::endl;
                    std::cerr << "Valid environments are: lucas, docker, aws, shanaya" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Missing environment name after " << arg << std::endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            displayHelp(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            displayHelp(argv[0]);
            return 1;
        }
    }

    std::cout << "Starting index node on port " << port << " in environment " << env << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    try {
        Index node(port, env);
        // Spin the loop, so Node doesn't die
        while(running) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
        
        std::cout << "Shutting down index node..." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error starting index node: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}