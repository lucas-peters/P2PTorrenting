#include "index.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <string>

using namespace torrent_p2p;

void displayHelp(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --port, -p <port>    Specify port number (default: 6883)\n";
    std::cout << "  --env, -e <env>      Specify environment (default: lucas)\n";
    std::cout << "                       Valid environments: lucas, docker, aws, shanaya\n";
    std::cout << "  --help, -h           Display this help message\n";
}

int main(int argc, char* argv[]) {
    // default vals
    int port = 6883;
    std::string env = "lucas";
    std::string ip = "None";
    bool running = true;
    
    // parsing cli
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
                if (env != "lucas" && env != "docker" && env != "aws" && env != "shanaya") {
                    std::cerr << "Invalid environment: " << env << std::endl;
                    std::cerr << "Valid environments are: lucas, docker, aws, shanaya" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Missing environment name after " << arg << std::endl;
                return 1;
            }
        } else if (arg == "--ip" || arg == "-i") {
            if (i + 1 < argc) {
                ip = argv[i + 1];
                i++;
            } else {
                std::cerr << "Missing ip after " << arg << std::endl;
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
        Index node(port, env, ip);
        // continuously spin so main loop doesn't exit
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