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
    std::cout << "  add <torrent_file>                - Add a torrent file to download\n";
    std::cout << "  magnet <magnet_link>              - Add a torrent via magnet link\n";
    std::cout << "  generate <file_path>              - Generate a torrent file from a file/directory\n";
    std::cout << "  status                            - Display status of all torrents\n";
    std::cout << "  search <info_hash>                - Search DHT network for peers with specific info hash\n";
    std::cout << "  connect <ip> <port>               - Connect to a specific bootstrap node\n";
    std::cout << "  dht                               - Display DHT statistics\n";
    std::cout << "  gossip                            - Send a test gossip message\n";
    std::cout << "  save                              - Save DHT state to file\n";
    std::cout << "  load                              - Load DHT state from file\n";
    std::cout << "  corrupt <% corruption>            - Corrupts all torrents in download path\n";
    std::cout << "  clear                             - Clear the console\n";
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
    } else if (command == "clear" || command == "cls") {
        // Clear screen - works on most terminals
        std::cout << "\033[2J\033[1;1H";
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
            std::cout << "Usage: search <info_hash>" << std::endl;
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
    } else if (command == "dht") {
        std::cout << client->getDHTStats() << std::endl;
    
    //  else if (command == "gossip") {
    //     std::cout << "Sending gossip message...\n";
    //     GossipMessage message;
            //     message.set_source_ip("127.0.0.1");
            //     message.set_source_port(port);
            //     lt::tcp::endpoint exclude(lt::make_address_v4("127.0.0.1"), port);
            //     client->gossip_->spreadMessage(message, exclude);
            // } 
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

    // Create a named pipe for commands
    std::string pipe_path = "/app/command_pipe";
    if (access(pipe_path.c_str(), F_OK) == -1) {
        if (mkfifo(pipe_path.c_str(), 0666) != 0) {
            std::cerr << "Failed to create command pipe at: " << pipe_path << std::endl;
        } else {
            std::cout << "Created command pipe at: " << pipe_path << std::endl;
        }
    }
    // PIPE THREAD, this is the only way to pipe commands to running docker containers
    std::atomic<bool> running{true};
    std::thread pipe_thread([&client, &running, pipe_path]() {
        // Set up polling for both stdin and the named pipe
        struct pollfd fds[2];
        fds[0].fd = STDIN_FILENO;  // stdin
        fds[0].events = POLLIN;
        
        // Open pipe for reading (non-blocking)
        int pipe_fd = open(pipe_path.c_str(), O_RDONLY | O_NONBLOCK);
        if (pipe_fd == -1) {
            std::cerr << "Failed to open command pipe for reading" << std::endl;
        } else {
            fds[1].fd = pipe_fd;
            fds[1].events = POLLIN;
        }
        
        // Buffer for commands
        std::string input;
        char buffer[1024];
        
        // Command processing loop
        while (running) {
            // Display prompt
            std::cout << "> ";
            std::cout.flush();
            
            // Use poll to wait for input from either stdin or pipe
            int ready = poll(fds, pipe_fd != -1 ? 2 : 1, 1000); // 1 second timeout
            
            if (ready > 0) {
                // Check stdin
                if (fds[0].revents & POLLIN) {
                    std::getline(std::cin, input);
                    // Process input as you already do...
                    processCommand(input, client, running);
                }
                
                // Check pipe
                if (pipe_fd != -1 && (fds[1].revents & POLLIN)) {
                    ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1);
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        input = buffer;
                        
                        // Remove trailing newline if present
                        if (!input.empty() && input.back() == '\n') {
                            input.pop_back();
                        }
                        
                        // Echo the command for clarity
                        std::cout << "\n> " << input << std::endl;
                        
                        // Process the command
                        processCommand(input, client, running);
                    }
                }
            }
        }
        
        // Close pipe if open
        if (pipe_fd != -1) {
            close(pipe_fd);
        }
    });

    // Display welcome message and commands
    std::cout << "\n===== P2P Torrenting Client =====\n";
    std::cout << "Client running on port: " << port << "\n";
    std::cout << "Type 'help' for available commands\n";
    std::cout << "================================\n\n";

    // MAIN THREAD, this is how to access the basic cli

    std::thread user_thread([&client, &running]() {
        // Command processing loop
        std::string input;
        while (running) {
            std::cout << "> ";
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
                std::cout << "Shutting down client..." << std::endl;
                running = false;
                break;
            } else if (command == "help") {
                displayHelp();
            } else if (command == "clear" || command == "cls") {
                // Clear screen - works on most terminals
                std::cout << "\033[2J\033[1;1H";
            } else if (command == "add") {
                std::string torrent_file;
                if (iss >> torrent_file ) {
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
                    std::cout << "Usage: search <info_hash>" << std::endl;
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
            }
            //  else if (command == "gossip") {
            //     std::cout << "Sending gossip message...\n";
            //     GossipMessage message;
            //     message.set_source_ip("127.0.0.1");
            //     message.set_source_port(port);
            //     lt::tcp::endpoint exclude(lt::make_address_v4("127.0.0.1"), port);
            //     client->gossip_->spreadMessage(message, exclude);
            // } 
            else if (command == "save") {
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
    });

    while (running) {
    // Only print stats every 30 seconds to avoid cluttering the console
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (running) {
            std::cout << "\n[DHT Stats Update]\n" << client->getDHTStats() << std::endl;
            std::cout << "> ";
            std::cout.flush(); // Make sure the prompt is displayed after stats
        }
    }

    // Clean up
    if (user_thread.joinable()) {
        user_thread.join();
    }
    if (pipe_thread.joinable()) {
        pipe_thread.join();
    }

    std::cout << "Client shutdown complete.\n";
    return 0;
}