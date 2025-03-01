#include "node.hpp"

namespace torrent_p2p {
Node::Node(int port) : port_(port), running_(false) {}

Node::Node(int port, const std::string& state_file) : port_(port), running_(false), state_file_(state_file) {
    loadDHTState(state_file);
}

Node::~Node() {
    stop();
}

void Node::stop() {
    running_ = false;
    saveDHTState();
    if (session_) {
        session_.reset();
    }
}


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

} // namespace torrent_p2p
