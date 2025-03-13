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
#include <mutex>

using namespace torrent_p2p;
namespace fs = std::filesystem;

// Function to display help information
void displayHelp() {
    std::cout << "\n===== P2P Torrenting Client - Command Reference =====\n";
    std::cout << "  add <torrent_file>                - Add a torrent file to download\n";
    std::cout << "  magnet <magnet_link>              - Add a torrent via magnet link\n";
    std::cout << "  generate <file_path>              - Generate a torrent file from a file/directory\n";
    std::cout << "  status                            - Display status of all torrents\n";
    std::cout << "  search <keyword>                  - Search for torrents by keyword\n";
    std::cout << "  dht                               - Display DHT statistics\n";
    std::cout << "  save                              - Save DHT state to file\n";
    std::cout << "  load <state_file>                 - Load DHT state from file\n";
    std::cout << "  corrupt <% corruption>            - Corrupts all torrents in download path\n";
    // std::cout << "  pause <name>                      - Pause a torrent by name\n";
    // std::cout << "  resume <name>                     - Resume a paused torrent by name\n";
    // std::cout << "  remove <name>                     - Remove a torrent by name\n";
    std::cout << "  help                              - Display this help message\n";
    std::cout << "  quit                              - Exit the application\n";
    std::cout << "====================================================\n";
}

void processCommand(const std::string& input, std::unique_ptr<Client>& client, std::atomic<bool>& running) {
    std::istringstream iss(input);
    std::string command;
    iss >> command;
    
    if (command.empty()) {
        return;
    }

    if (command == "quit" || command == "exit") {
        std::cout << "Shutting down client..." << std::endl;
        running = false;
        return;
    } else if (command == "help") {
        displayHelp();
    } else if (command == "add") {
        std::string torrent_file;
        if (iss >> torrent_file) {
            try {
                std::cout << "Adding torrent: " << torrent_file << std::endl;
                client->addTorrent(torrent_file);
            } catch (const std::exception& e) {
                std::cerr << "Error adding torrent: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Usage: add <torrent_file>" << std::endl;
        }
    } else if (command == "magnet") {
        std::string info_hash;
        if (iss >> info_hash) {
            try {
                std::cout << "Adding magnet link with info hash: " << info_hash << std::endl;
                client->addMagnet(info_hash);
            } catch (const std::exception& e) {
                std::cerr << "Error adding magnet link: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Usage: magnet <info_hash> <save_path>" << std::endl;
        }
    } else if (command == "status") {
        client->printStatus();
    } else if (command == "generate") {
        std::string file_path;
        if (iss >> file_path) {
            try {
                std::cout << "Generating torrent file for: " << file_path << std::endl;
                client->generateTorrentFile(file_path);
                std::cout << "Torrent file generated successfully." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error generating torrent file: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Usage: generate <file_path>" << std::endl;
        }
    } else if (command == "search") {
        std::string keyword;
        if (iss >> keyword) {
            try {
                std::cout << "Searching DHT for keyword: " << keyword << std::endl;
                client->searchIndex(keyword);
            } catch (const std::exception& e) {
                std::cerr << "Error searching DHT: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Usage: search <keyword>" << std::endl;
        }
    } else if (command == "dht") {
        std::cout << client->getDHTStats() << std::endl;
    } else if (command == "save") {
        if (client->saveDHTState()){
            std::cout << "DHT state saved successfully." << std::endl;
        } else {
            std::cerr << "Failed to save DHT state." << std::endl;
        }
    } else if (command == "load") {
        std::string load_file;
        if (iss >> load_file) {
            try {
                std::cout << "Loading DHT state from: " << load_file << std::endl;
                if (client->loadDHTState(load_file)) {
                    std::cout << "DHT state loaded successfully." << std::endl;
                } else {
                    std::cerr << "Failed to load DHT state." << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error loading DHT state: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Usage: load <state_file>" << std::endl;
        }
    } else if (command == "corrupt") {
        std::string corrupt_percentage;
        if (iss >> corrupt_percentage) {
            std::cout << "Corrupting all torrents in session" << std::endl;
            client->corruptAllTorrents(std::stof(corrupt_percentage));
        } else {
            std::cout << "Usage: corrupt <corrupt_percentage>" << std::endl;
        }
    } else {
        std::cout << "Unknown command: " << command << std::endl;
        std::cout << "Type 'help' for available commands" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    
    // Parse command line arguments
    int port = 6882;
    std::string state_file = "";
    bool load_state = false;
    std::string env = "lucas";
    std::string ip = "None";
    
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
        } else if (arg == "--state" || arg == "-s") {
            if (i + 1 < argc) {
                state_file = argv[i + 1];
                load_state = true;
                i++;
            } else {
                std::cerr << "Missing state file path after " << arg << std::endl;
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
        } else if (arg == "--ip" || arg == "-i") {
            if (i + 1 < argc) {
                ip = argv[i + 1];
                i++;
            } else {
                std::cerr << "Missing ip after " << arg << std::endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --port, -p <port>    Specify port number (default: 6882)\n";
            std::cout << "  --state, -s <file>   Load DHT state from file\n";
            std::cout << "  --env, -e <env>      Specify environment (default: lucas)\n";
            std::cout << "                       Valid environments: lucas, docker, aws, shanaya\n";
            std::cout << "  --help, -h           Display this help message\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Use --help for more information.\n";
            return 1;
        }
    }

    std::cout << "Starting client on port: " << port << " in environment " << env << std::endl;

    // Initialize client
    std::unique_ptr<Client> client;
    
    if (load_state && !state_file.empty()) {
        std::cout << "Loading DHT state from: " << state_file << std::endl;
        client = std::make_unique<Client>(port, env, ip, state_file);
    } else {
        std::cout << "Client 3 args" << std::endl;
        client = std::make_unique<Client>(port, env, ip);
    }

    // Create a named pipe for commands, will only work in docker
    std::string pipe_path = "/app/command_pipe";
    if (access(pipe_path.c_str(), F_OK) == -1) {
        if (mkfifo(pipe_path.c_str(), 0666) != 0) {
            std::cerr << "Failed to create command pipe at: " << pipe_path << std::endl;
        } else {
            std::cout << "Created command pipe at: " << pipe_path << std::endl;
        }
    }
    
    // mutex for stdin/stdout
    std::mutex console_mutex;

    // Display welcome message and commands
    std::cout << "\n===== P2P Torrenting Client =====\n";
    std::cout << "Client running on port: " << port << "\n";
    std::cout << "Type 'help' for available commands\n";
    std::cout << "================================\n\n";
    
    // PIPE THREAD, this is the only way to pipe commands to running docker containers
    std::atomic<bool> running{true};
    std::thread pipe_thread([&client, &running, pipe_path, &console_mutex]() {
        // Set up file descriptors for select
        fd_set master_fds, read_fds;
        FD_ZERO(&master_fds);
        
        // Add stdin to the set
        FD_SET(STDIN_FILENO, &master_fds);
        int max_fd = STDIN_FILENO;
        
        // Open pipe for reading (non-blocking)
        int pipe_fd = open(pipe_path.c_str(), O_RDONLY | O_NONBLOCK);
        if (pipe_fd == -1) {
            std::cerr << "Failed to open command pipe for reading" << std::endl;
        } else {
            FD_SET(pipe_fd, &master_fds);
            max_fd = std::max(max_fd, pipe_fd);
        }
        
        // Buffer for commands
        char buffer[1024];
        
        // Command processing loop
        while (running) {
            // Copy the master set to the read set
            read_fds = master_fds;
            
            // Set timeout for select (100ms)
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; // 100ms
            
            // Wait for input on any of the file descriptors
            int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
            
            if (ready == -1) {
                // Error in select
                std::cerr << "Error in select: " << strerror(errno) << std::endl;
                continue;
            } else if (ready == 0) {
                continue;
            }
            
            // Check if stdin has input
            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                std::string input;
                {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    if (!std::getline(std::cin, input)) {
                        // EOF or error on stdin
                        if (std::cin.eof()) {
                            std::cout << "End of input, exiting..." << std::endl;
                            running = false;
                            break;
                        }
                        continue;
                    }
                }
                if (!input.empty()) {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    processCommand(input, client, running);
                    std::cout << "> " << std::flush;
                } else {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "> " << std::flush;
                }
            }
            
            // Check if pipe has input
            if (pipe_fd != -1 && FD_ISSET(pipe_fd, &read_fds)) {
                ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1);
                
                if (bytes_read > 0) {
                    // Null-terminate the buffer
                    buffer[bytes_read] = '\0';
                    std::string input(buffer);
                    
                    // Remove trailing newline if present
                    if (!input.empty() && input.back() == '\n') {
                        input.pop_back();
                    }
                    
                    // Process the command with mutex protection
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "\nReceived command from pipe: " << input << std::endl;
                    processCommand(input, client, running);
                    std::cout << "> " << std::flush;
                } else if (bytes_read == 0) {
                    std::cerr << "Command pipe closed" << std::endl;
                    close(pipe_fd);
                    FD_CLR(pipe_fd, &master_fds);
                    pipe_fd = -1;
                } else {
                    // Error reading from pipe
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        std::cerr << "Error reading from pipe: " << strerror(errno) << std::endl;
                        close(pipe_fd);
                        FD_CLR(pipe_fd, &master_fds);
                        pipe_fd = -1;
                    }
                }
            }
        }
        
        // Close pipe if open
        if (pipe_fd != -1) {
            close(pipe_fd);
        }
    });

    while (running) {
        // Only print stats every 30 seconds to avoid cluttering the console
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (running) {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "\n[DHT Stats Update]\n" << client->getDHTStats() << std::endl;
            std::cout << "> ";
            std::cout.flush();
        }
    }
    if (pipe_thread.joinable()) {
        pipe_thread.join();
    }

    std::cout << "Client terminated\n";
    return 0;
}