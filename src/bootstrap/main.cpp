#include "bootstrap_node.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

using namespace torrent_p2p;

// Track active bootstrap nodes
std::map<int, pid_t> bootstrap_nodes;
int port_start = 5000;
int port_end = 5100;
int next_port = port_start;

// Graceful shutdown handler
volatile sig_atomic_t running = 1;
void signal_handler(int signal) {
    std::cout << "\n[Main] Received signal " << signal << ", shutting down..." << std::endl;
    running = 0;
}

// Get an available port
int get_available_port() {
    while (bootstrap_nodes.find(next_port) != bootstrap_nodes.end()) {
        next_port++;
        if (next_port > port_end) next_port = port_start; // Wrap around
    }
    return next_port;
}

// Start a new bootstrap node
void add_bootstrap_node() {
    int port = get_available_port();
    pid_t pid = fork();

    if (pid == 0) {
        // Child process: Execute bootstrap node
        std::string cmd = "./p2p_bootstrap " + std::to_string(port);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), NULL);
        exit(1);
    } else if (pid > 0) {
        // Parent process: Store the new node
        bootstrap_nodes[port] = pid;
        std::cout << "[Main] Started bootstrap node on port " << port << " (PID: " << pid << ")\n";
    } else {
        std::cerr << "[Main] Error: Failed to fork process\n";
    }
}

// Kill a bootstrap node
void remove_bootstrap_node(int port) {
    auto it = bootstrap_nodes.find(port);
    if (it != bootstrap_nodes.end()) {
        kill(it->second, SIGTERM);
        waitpid(it->second, NULL, 0);
        bootstrap_nodes.erase(it);
        std::cout << "[Main] Removed bootstrap node on port " << port << "\n";
    } else {
        std::cerr << "[Main] Error: No bootstrap node on port " << port << "\n";
    }
}

// Simulate failure by killing a random node
void simulate_failure() {
    if (bootstrap_nodes.empty()) {
        std::cerr << "[Main] No bootstrap nodes to fail.\n";
        return;
    }
    auto it = bootstrap_nodes.begin();
    int port = it->first;
    remove_bootstrap_node(port);
    std::cout << "[Main] Simulated failure on node running at port " << port << "\n";
}

// List active bootstrap nodes
void list_bootstrap_nodes() {
    if (bootstrap_nodes.empty()) {
        std::cout << "[Main] No active bootstrap nodes.\n";
        return;
    }
    std::cout << "[Main] Active Bootstrap Nodes:\n";
    for (const auto& [port, pid] : bootstrap_nodes) {
        std::cout << "  - Port: " << port << ", PID: " << pid << "\n";
    }
}

// Main loop
int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string command;
    while (running) {
        std::cout << "\nCommands: add | remove <port> | list | fail | exit\n> ";
        std::cin >> command;

        if (command == "add") {
            add_bootstrap_node();
        } else if (command == "remove") {
            int port;
            std::cin >> port;
            remove_bootstrap_node(port);
        } else if (command == "list") {
            list_bootstrap_nodes();
        } else if (command == "fail") {
            simulate_failure();
        } else if (command == "exit") {
            for (const auto& [port, pid] : bootstrap_nodes) {
                kill(pid, SIGTERM);
                waitpid(pid, NULL, 0);
            }
            bootstrap_nodes.clear();
            break;
        } else {
            std::cerr << "[Main] Invalid command\n";
        }
    }

    return 0;
}
