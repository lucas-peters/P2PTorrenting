#include "node.hpp"

// Add explicit include for protobuf-generated headers
#include "gossip.pb.h"
#include "index.pb.h"

using json = nlohmann::json;

namespace torrent_p2p {
Node::Node(int port, const std::string& env) : port_(port), running_(false) {
    loadEnvConfig(env);
    setIP(env);
}

Node::Node(int port, const std::string& env, const std::string& state_file) : port_(port), running_(false), state_file_(state_file) {
    loadEnvConfig(env);
    setIP(env);
    loadDHTState(state_file);
}

void Node::loadEnvConfig(const std::string& env) {
    std::string config_path;
    
    // Check if CONFIG_PATH environment variable is set (highest priority)
    const char* envPath = std::getenv("CONFIG_PATH");
    if (envPath) {
        config_path = envPath;
        std::cout << "Using config path from environment variable: " << config_path << std::endl;
    } else if (env == "aws" || env == "docker") {
        // In Docker/AWS, use the container path
        config_path = "/app/config.json";
        std::cout << "Using Docker/AWS config path: " << config_path << std::endl;
    } else {
        // For local development, use the project root from CMake
        config_path = std::string(PROJECT_ROOT) + "/config.json";
        std::cout << "Using local config path: " << config_path << std::endl;
    }
    
    try {
        // Open the config file
        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << config_path << std::endl;
            return;
        }
        
        // Parse the JSON
        json config;
        file >> config;
        
        // Check if the environment exists
        if (!config["environments"].contains(env)) {
            std::cerr << "Environment not found: " << env << std::endl;
            return;
        }
        
        // Get the environment data
        const auto& env_data = config["environments"][env];
        
        // Parse bootstrap nodes
        bootstrap_nodes_.clear();
        if (env_data.contains("bootstrap_nodes") && env_data["bootstrap_nodes"].is_array()) {
            for (const auto& node : env_data["bootstrap_nodes"]) {
                if (node.is_array() && node.size() == 2 && 
                    node[0].is_string() && node[1].is_number()) {
                    bootstrap_nodes_.emplace_back(node[0].get<std::string>(), node[1].get<int>());
                } else {
                    std::cerr << "Invalid bootstrap node format" << std::endl;
                }
            }
        }
        
        // Parse index nodes
        index_nodes_.clear();
        if (env_data.contains("index_nodes") && env_data["index_nodes"].is_array()) {
            for (const auto& node : env_data["index_nodes"]) {
                if (node.is_array() && node.size() == 2 && 
                    node[0].is_string() && node[1].is_number()) {
                    index_nodes_.emplace_back(node[0].get<std::string>(), node[1].get<int>());
                } else {
                    std::cerr << "Invalid index node format" << std::endl;
                }
            }
        }
        
        // Print loaded configuration
        std::cout << "Loaded configuration for environment: " << env << std::endl;
        std::cout << "Bootstrap nodes:" << std::endl;
        for (const auto& node : bootstrap_nodes_) {
            std::cout << "  " << node.first << ":" << node.second << std::endl;
        }
        std::cout << "Index nodes:" << std::endl;
        for (const auto& node : index_nodes_) {
            std::cout << "  " << node.first << ":" << node.second << std::endl;
        }
    }
    catch (const json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
    }
}

Node::~Node() {
    stop();
}

void Node::start() {
    if (!session_) {
        initializeSession();
    }

    std::cout << "Connecting to DHT bootstrap nodes..." << std::endl;
    connectToDHT(bootstrap_nodes_);

    try {
        // Make sure the session is fully initialized before creating Gossip
        std::cout << "Waiting for DHT to initialize..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Give DHT time to initialize
        
        std::cout << "Creating Gossip object..." << std::endl;
        gossip_ = std::make_unique<Gossip>(*session_, port_ + 1000);
        std::cout << "Gossip object created successfully" << std::endl;

        std::cout << " Creating Messenger" << std::endl;
        messenger_ = std::make_unique<Messenger>(*session_, port_ + 2000, ip_);
        std::cout << "Messenger object created successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during Gossip initialization: " << e.what() << std::endl;
    }
    running_ = true;
}

void Node::initializeSession() {
    lt::settings_pack pack;
    pack.set_int(lt::settings_pack::alert_mask,
        lt::alert::status_notification
        | lt::alert::error_notification
        | lt::alert::dht_notification
        | lt::alert::port_mapping_notification
        | lt::alert::dht_log_notification
        | lt::alert::piece_progress_notification
        | lt::alert::storage_notification
        | lt::alert_category::block_progress);

    // Force Highest Level Encryption, ChaCha20 instead of RC4
    pack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_forced);
    pack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_forced);
    
    // Enable DHT but don't give it access to bitTorrent bootstrap nodes, we are implementing a closed system
    pack.set_bool(lt::settings_pack::enable_dht, true);
    pack.set_str(lt::settings_pack::dht_bootstrap_nodes, "");  // Disable external bootstrap nodes
    pack.set_int(lt::settings_pack::dht_announce_interval, 5);
    pack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
    pack.set_bool(lt::settings_pack::enable_incoming_utp, true);
    pack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
    pack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
    
    // Disable IP restrictions for local testing, by default it doesn't allow duplicate ips. i.e: localhost
    pack.set_bool(lt::settings_pack::dht_restrict_routing_ips, false);
    pack.set_bool(lt::settings_pack::dht_restrict_search_ips, false);
    pack.set_int(lt::settings_pack::dht_max_peers_reply, 100);
    pack.set_bool(lt::settings_pack::dht_ignore_dark_internet, false);
    pack.set_int(lt::settings_pack::dht_max_fail_count, 5);
    
    // listen on all
    pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:" + std::to_string(port_));
    
    session_ = std::make_unique<lt::session>(pack);
}


void Node::stop() {
    running_ = false;
    saveDHTState();
    if (session_) {
        session_.reset();
    }
}

// still need to test this
bool Node::saveDHTState() const {
    if (!session_) {
        return false;
    }
    
    try {
        // Get the session state with only DHT state
        lt::session_params params = session_->session_state();
        
        // Serialize the session params to a buffer
        std::vector<char> buffer = lt::write_session_params_buf(params, lt::session::save_dht_state);
        
        std::ofstream file(state_file_, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open state file for writing: " + state_file_);
            return false;
        }

        file.write(buffer.data(), buffer.size());
        std::cout << "Saved DHT state to " << state_file_ << std::endl;
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving DHT state: " << e.what() << std::endl;
        return false;
    }

}
// still need to test this
bool Node::loadDHTState(const std::string& state_file) {
    if (!session_) {
        return false;
    }
    try {
        // Check if the file exists
        std::ifstream file(state_file, std::ios::binary);
        if (!file) {
            std::cerr << "DHT state file not found: " << state_file << std::endl;
            return false;
        }
        
        // Read the file into a buffer
        file.seekg(0, std::ios::end);
        std::streampos size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<char> buffer(size);
        file.read(buffer.data(), size);
        file.close();

        // Parse the buffer into session_params
        lt::session_params params = lt::read_session_params(lt::span<char const>(buffer.data(), buffer.size()), 
                                                          lt::session::save_dht_state);
        
        // Apply the DHT state to the session
        session_->apply_settings(params.settings);
        session_->set_dht_state(params.dht_state);
        
        std::cout << "DHT state loaded from " << state_file << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading DHT state: " << e.what() << std::endl;
        return false;
    }
}

std::pair<std::string, int> Node::getEndpoint() const {
    if (!session_) {
        return {"not started", 0};
    }
    
    return {"127.0.0.1", port_};
}

std::string Node::getDHTStats() const {
    if (!session_) {
        return "Session not initialized";
    }
    
    session_->post_dht_stats();
    return "DHT stats requested";
}

void Node::connectToDHT(const std::vector<std::pair<std::string, int>>& bootstrap_nodes) {
    if (!session_) {
        std::cout << "Session not initialized" << std::endl;
        return;
    }

    // Add bootstrap nodes to routing table
    for (const auto& node : bootstrap_nodes) {
        std::cout << "[Node:" << port_ << "] Adding bootstrap node: " << node.first << ":" << node.second << std::endl;
        
        try {
            // First, check if the bootstrap node is reachable
            std::cout << "[Node:" << port_ << "] Checking if bootstrap node is reachable..." << std::endl;
            lt::udp::endpoint ep(lt::make_address_v4(node.first), node.second);
            
            // Add the node to the DHT routing table
            session_->add_dht_node(std::make_pair(node.first, node.second));
            
            // Force immediate DHT lookup to this node
            std::cout << "[Node:" << port_ << "] Sending direct DHT request to " << node.first << ":" << node.second << std::endl;
            session_->dht_direct_request(ep, lt::entry{}, lt::client_data_t{});
            
            // Also announce ourselves with a generated hash
            lt::sha1_hash hash;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> dis;
            for (int i = 0; i < 5; i++) {
                reinterpret_cast<uint32_t*>(hash.data())[i] = dis(gen);
            }
            std::cout << "[Node:" << port_ << "] Announcing to DHT with hash: " << hash << std::endl;
            session_->dht_announce(hash, port_);
            
            // Also try to get peers for this hash to force DHT activity
            std::cout << "[Node:" << port_ << "] Getting peers for hash: " << hash << std::endl;
            session_->dht_get_peers(hash);
        } catch (const std::exception& e) {
            std::cerr << "[Node:" << port_ << "] Error connecting to node " << node.first << ":" << node.second 
                      << " - " << e.what() << std::endl;
        }
    }
    
    // Start periodic DHT lookups //like a heartbeat
    std::thread([this]() {
        while (running_) {
            std::cout << "[Node:" << port_ << "] Running periodic DHT maintenance..." << std::endl;
            if (session_) {
                try {
                    // Request DHT stats to see what's happening
                    session_->post_dht_stats();
                    
                    // Try to get peers for an empty hash to force DHT activity
                    session_->dht_get_peers(lt::sha1_hash());
                    
                    // Get nodes from our DHT routing table
                    lt::session_params params = session_->session_state();
                    if (params.dht_state.nodes.empty()) {
                        std::cout << "[Node:" << port_ << "] WARNING: No DHT nodes in routing table!" << std::endl;
                        
                        // Try to reconnect to bootstrap nodes
                        for (const auto& node : bootstrap_nodes_) {
                            std::cout << "[Node:" << port_ << "] Re-adding bootstrap node: " << node.first << ":" << node.second << std::endl;
                            session_->add_dht_node(std::make_pair(node.first, node.second));
                        }
                    } else {
                        std::cout << "[Node:" << port_ << "] DHT routing table has " << params.dht_state.nodes.size() << " nodes:" << std::endl;
                        for (auto & node : params.dht_state.nodes) {
                            std::cout << "[Node:" << port_ << "] DHT node: " << node.address() << ":" << node.port() << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[Node:" << port_ << "] Error in DHT maintenance: " << e.what() << std::endl;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();
}

void Node::setIP(const std::string& env) {
    // get ip from env if running on aws or docker
    if (env == "aws" || env == "docker") {
        const char* env_ip = std::getenv("PUBLIC_IP");
        if (env_ip && *env_ip) {
            std::cout << "Using public IP from environment: " << env_ip << std::endl;
            ip_ = std::string(env_ip);
            return;
        }
    }
    
    // If running purely local, ip will be the same as the bootstrap node
    ip_ = bootstrap_nodes_[0].first;
    std::cout << "SET ip_: " << ip_ << std::endl;
}
// lt::sha1_hash Node::getMyNodeId() const {
//     lt::sha1_hash node_id;
    
//     // Try method 1: Get from session state
//     try {
//         lt::entry session_state = session_->save_state();
//         if (session_state.find_key("dht state")) {
//             lt::entry dht_state = session_state["dht state"];
//             if (dht_state.find_key("node-id")) {
//                 std::string node_id_str = dht_state["node-id"].string();
//                 if (node_id_str.size() == 20) {
//                     std::memcpy(node_id.data(), node_id_str.data(), 20);
//                     return node_id;
//                 }
//             }
//         }
//     } catch (const std::exception& e) {
//         std::cerr << "Error getting node ID from session state: " << e.what() << std::endl;
//     }
    
//     // If we couldn't get it from session state, try other methods
//     // (Add additional methods here based on your specific implementation)
    
//     // If all else fails, return an empty hash
//     return node_id;
// }

} // namespace torrent_p2p
