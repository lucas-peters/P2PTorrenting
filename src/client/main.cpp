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
#include <drogon/drogon.h>

using namespace torrent_p2p;
using namespace drogon;
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
    std::cout << "  magnet <info_hash> <save_path>    - Add a torrent via magnet link\n";
    std::cout << "  generate <file_path>              - Generate a torrent file from a file/directory\n";
    std::cout << "  status                            - Display status of all torrents\n";
    std::cout << "  search <info_hash>                - Search DHT network for peers with specific info hash\n";
    std::cout << "  connect <ip> <port>               - Connect to a specific bootstrap node\n";
    std::cout << "  dht                               - Display DHT statistics\n";
    std::cout << "  gossip                            - Send a test gossip message\n";
    std::cout << "  save <state_file>                 - Save DHT state to file\n";
    std::cout << "  load <state_file>                 - Load DHT state from file\n";
    std::cout << "  clear                             - Clear the console\n";
    std::cout << "  help                              - Display this help message\n";
    std::cout << "  quit                              - Exit the application\n";
    std::cout << "====================================================\n";
}


// Function to start REST API in a separate thread
void startRestApi(std::unique_ptr<Client>& client) {
    app().registerHandler("/*",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp->setBody("CORS preflight OK");
            callback(resp);
        },
        {Options} // This ensures that all OPTIONS requests are handled
    );

    app().registerHandler("/hello",
        [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setBody("Hello, World!");
            callback(resp);
        },
        {drogon::Get}
    );
    
    app().registerHandler("/add-torrent",
        [&client](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("file")) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(HttpStatusCode::k400BadRequest);
                resp->setBody("Missing 'file' parameter");
                callback(resp);
                return;
            }

            std::string file_path = (*json)["file"].asString();
            client->addTorrent(file_path);

            auto resp = HttpResponse::newHttpResponse();

            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody("Torrent added successfully");

            callback(resp);
        },
        {Post}
    );

    app().registerHandler("/generate-torrent",
        [&client](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("path")) {
                auto resp = HttpResponse::newHttpResponse();

                resp->setStatusCode(HttpStatusCode::k400BadRequest);
                resp->setBody("Missing 'path' parameter");
                callback(resp);
                return;
            }

            std::string path = (*json)["path"].asString();
            client->generateTorrentFile(path);

            auto resp = HttpResponse::newHttpResponse();

            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
                
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody("Torrent file generated");
            callback(resp);
        },
        {Post}
    );

    app().registerHandler("/generate-torrent",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            callback(resp);
        },
        {Options} // Handles OPTIONS requests explicitly for /add-torrent
    );

    app().registerHandler("/add-torrent",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            callback(resp);
        },
        {Options} // Handles OPTIONS requests explicitly for /add-torrent
    );

    app().registerHandler("/status",
        [&client](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback) {
            std::string res = client->getStatus();
            auto resp = HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody(res);
            callback(resp);
        },
        {Get}
    );

    app().registerHandler("/search/{info_hash}",
        [&client](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback, std::string info_hash) {
            client->searchDHT(info_hash);
            auto resp = HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody("DHT search initiated");
            callback(resp);
        },
        {Get}
    );

    app().registerHandler("/dht-stats",
        [&client](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody(client->getDHTStats());
            callback(resp);
        },
        {Get}
    );

    app().addListener("0.0.0.0", 8080).run();
}

int main(int argc, char* argv[]) {
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
    
    // Display welcome message and commands
    std::cout << "\n===== P2P Torrenting Client =====\n";
    std::cout << "Client running on port: " << port << "\n";
    std::cout << "Type 'help' for available commands\n";
    std::cout << "================================\n\n";

    // Starting a background thread that prints DHT stats
    std::atomic<bool> running{true};

    std::thread apiThread([&client]() {
        startRestApi(client); 
    });    

    // CLI loop
    std::thread user_thread([&client, &running]() {
        // Command processing loop
        std::string input;
        while (running) {
            std::cout << "> ";
            std::cout.flush();
            std::getline(std::cin, input);

            std::istringstream iss(input);
            std::string command;
            iss >> command;

            if (command.empty()) {
                continue;
            }

            std::transform(command.begin(), command.end(), command.begin(), 
                        [](unsigned char c){ return std::tolower(c); });

            if (command == "quit" || command == "exit") {
                std::cout << "Shutting down client...\n";
                running = false;
                app().quit();  // Stop the REST API server
                break;
            } else if (command == "help") {
                displayHelp();
            } else if (command == "clear" || command == "cls") {
                std::cout << "\033[2J\033[1;1H";  // Clear terminal screen
            } else if (command == "add") {
                std::string torrent_file;
                if (iss >> torrent_file ) {
                    try {
                        std::cout << "Adding torrent: " << torrent_file << "\n";
                        client->addTorrent(torrent_file);
                    } catch (const std::exception& e) {
                        std::cerr << "Error adding torrent: " << e.what() << "\n";
                    }
                } else {
                    std::cout << "Usage: add <torrent_file>\n";
                }
            } else if (command == "magnet") {
                std::string info_hash, save_path;
                if (iss >> info_hash >> save_path) {
                    try {
                        std::cout << "Adding magnet link with info hash: " << info_hash << "\n";
                        std::cout << "Save path: " << save_path << "\n";
                        client->addMagnet(info_hash, save_path);
                    } catch (const std::exception& e) {
                        std::cerr << "Error adding magnet link: " << e.what() << "\n";
                    }
                } else {
                    std::cout << "Usage: magnet <info_hash> <save_path>\n";
                }
            } else if (command == "status") {
                client->printStatus();
            } else if (command == "generate") {
                std::string file_path;
                if (iss >> file_path) {
                    try {
                        std::cout << "Generating torrent file for: " << file_path << "\n";
                        client->generateTorrentFile(file_path);
                        std::cout << "Torrent file generated successfully.\n";
                    } catch (const std::exception& e) {
                        std::cerr << "Error generating torrent file: " << e.what() << "\n";
                    }
                } else {
                    std::cout << "Usage: generate <file_path>\n";
                }
            } else if (command == "search") {
                std::string keyword;
                if (iss >> keyword) {
                    try {
                        std::cout << "Searching DHT for keyword: " << keyword << "\n";
                        client->searchIndex(keyword);
                    } catch (const std::exception& e) {
                        std::cerr << "Error searching DHT: " << e.what() << "\n";
                    }
                } else {
                    std::cout << "Usage: search <info_hash>\n";
                }
            } 
            else if (command == "dht") {
                std::cout << client->getDHTStats() << std::endl;
            }
            else if (command == "save") {
                std::string save_file;
                if (iss >> save_file) {
                    try {
                        std::cout << "Saving DHT state to: " << save_file << "\n";
                        if (client->saveDHTState()) {
                            std::cout << "DHT state saved successfully.\n";
                        } else {
                            std::cerr << "Failed to save DHT state.\n";
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error saving DHT state: " << e.what() << "\n";
                    }
                } else {
                    std::cout << "Usage: save <state_file>\n";
                }
            } else if (command == "load") {
                std::string load_file;
                if (iss >> load_file) {
                    try {
                        std::cout << "Loading DHT state from: " << load_file << "\n";
                        if (client->loadDHTState(load_file)) {
                            std::cout << "DHT state loaded successfully.\n";
                        } else {
                            std::cerr << "Failed to load DHT state.\n";
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error loading DHT state: " << e.what() << "\n";
                    }
                } else {
                    std::cout << "Usage: load <state_file>\n";
                }
            } else {
                std::cout << "Unknown command: " << command << "\n";
                std::cout << "Type 'help' for available commands\n";
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

    std::cout << "Client shutdown complete.\n";
    return 0;
}