#include "gossip.hpp"
#include <libtorrent/extensions/ut_metadata.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bdecode.hpp>
#include <libtorrent/hex.hpp>
#include <libtorrent/alert_types.hpp>
#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>

namespace torrent_p2p {

// ReputationMessage implementation
lt::entry ReputationMessage::to_entry() const {
    lt::entry e(lt::entry::dictionary_t);
    e["p"] = peer_id;
    e["d"] = reputation_delta;
    e["i"] = static_cast<std::int64_t>(message_id);
    e["s"] = timestamp;
    e["o"] = source_id;
    e["ttl"] = ttl;
    return e;
}

ReputationMessage ReputationMessage::from_entry(const lt::entry& e) {
    ReputationMessage msg;
    msg.peer_id = e["p"].string();
    msg.reputation_delta = e["d"].integer();
    msg.message_id = static_cast<std::uint32_t>(e["i"].integer());
    msg.timestamp = e["s"].integer();
    msg.source_id = e["o"].string();
    msg.ttl = e["ttl"].integer();
    return msg;
}

// GossipExtension implementation
GossipExtension::GossipExtension(lt::torrent_handle th)
    : m_torrent(th)
    , m_rng(std::random_device{}()) {
    std::cout << "Gossip extension initialized for torrent" << std::endl;
}

GossipExtension::~GossipExtension() {
    std::cout << "Gossip extension shutting down" << std::endl;
}

std::shared_ptr<lt::peer_plugin> GossipExtension::new_connection(
    lt::peer_connection_handle const& pc) {
    std::cout << "New peer connection: " << pc.remote() << std::endl;
    return std::make_shared<GossipPeerPlugin>(pc, shared_from_this());
}

bool GossipExtension::process_gossip_message(const ReputationMessage& msg, const lt::peer_connection_handle& source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if we've already seen this message
    if (m_seen_messages.count(msg.message_id) > 0) {
        return false;
    }
    
    // Add to seen messages
    m_seen_messages.insert(msg.message_id);
    
    // Update the reputation
    update_peer_reputation(msg.peer_id, msg.reputation_delta);
    
    // Propagate to other peers if TTL > 0
    if (msg.ttl > 0) {
        propagate_message(msg, source);
    }
    
    // Clean up old messages periodically
    clean_old_messages();
    
    return true;
}

void GossipExtension::create_reputation_update(const std::string& peer_id, std::int64_t delta) {
    ReputationMessage msg;
    msg.peer_id = peer_id;
    msg.reputation_delta = delta;
    msg.message_id = static_cast<std::uint32_t>(std::random_device{}());
    msg.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    msg.source_id = "self"; // TODO: Use actual node ID
    msg.ttl = 10;
    
    // Process locally first - directly update reputation without propagating
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Add to seen messages
    m_seen_messages.insert(msg.message_id);
    
    // Update the reputation
    update_peer_reputation(msg.peer_id, msg.reputation_delta);
    
    // Now propagate to peers (pass a null source since this is a local message)
    propagate_message(msg, lt::peer_connection_handle{});
}

std::int64_t GossipExtension::get_peer_reputation(const std::string& peer_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_reputations.find(peer_id);
    if (it != m_reputations.end()) {
        return it->second;
    }
    
    return 0; // Default reputation
}

std::shared_ptr<lt::torrent_plugin> GossipExtension::create(lt::torrent_handle const& th, void*) {
    return std::make_shared<GossipExtension>(th);
}

void GossipExtension::propagate_message(const ReputationMessage& msg, const lt::peer_connection_handle& source) {
    // Create a copy of the message with decremented TTL
    ReputationMessage forwarded_msg = msg;
    forwarded_msg.ttl--;
    
    if (forwarded_msg.ttl <= 0) {
        return; // Don't forward if TTL is exhausted
    }
    
    try {
        // Get all peers connected to this torrent
        std::vector<lt::peer_info> peer_info;
        m_torrent.get_peer_info(peer_info);
        
        // We'll manually create a list of peers, excluding the source if it's valid
        for (const auto& pi : peer_info) {
            // Skip if this is the excluded peer
            // Only check if source has a valid address
            bool is_excluded = false;
            try {
                if (source.remote().address().to_string() != "0.0.0.0" && 
                    pi.ip == source.remote()) {
                    is_excluded = true;
                }
            } catch (std::exception&) {
                // If we get an exception, the handle is likely invalid
                is_excluded = false;
            }
            
            if (!is_excluded) {
                // In a real implementation, you would get the peer_connection_handle
                // Since we can't directly get it, we'll just log for now
                std::cout << "Would forward to peer: " << pi.ip << std::endl;
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Error forwarding gossip message: " << e.what() << std::endl;
    }
}

std::vector<lt::peer_connection_handle> GossipExtension::select_random_peers(
    int count, const lt::peer_connection_handle& exclude) {
    std::vector<lt::peer_connection_handle> result;
    
    try {
        // Get all peers connected to this torrent
        std::vector<lt::peer_info> peer_info;
        m_torrent.get_peer_info(peer_info);
        
        // Convert to peer_connection_handles and filter out the excluded peer
        std::vector<lt::peer_connection_handle> all_peers;
        for (const auto& pi : peer_info) {
            // Skip if this is the excluded peer
            // Check if the exclude handle has a valid address by trying to access it
            bool is_excluded = false;
            try {
                // If we can get the remote address and it matches, exclude it
                if (exclude.remote().address().to_string() != "0.0.0.0" && 
                    pi.ip == exclude.remote()) {
                    is_excluded = true;
                }
            } catch (std::exception&) {
                // If we get an exception, the handle is likely invalid
                is_excluded = false;
            }
            
            if (!is_excluded) {
                // In a real implementation, we would get the peer_connection_handle
                // Since we can't directly get it, we'll just log for now
                std::cout << "Would select peer: " << pi.ip << std::endl;
            }
        }
        
        // If we have fewer peers than requested, just return all of them
        if (all_peers.size() <= static_cast<size_t>(count)) {
            return all_peers;
        }
        
        // Randomly select 'count' peers
        std::shuffle(all_peers.begin(), all_peers.end(), m_rng);
        result.assign(all_peers.begin(), all_peers.begin() + count);
    } catch (std::exception& e) {
        std::cerr << "Error selecting random peers: " << e.what() << std::endl;
    }
    
    return result;
}

void GossipExtension::update_peer_reputation(const std::string& peer_id, std::int64_t delta) {
    // Update the reputation value
    auto it = m_reputations.find(peer_id);
    if (it != m_reputations.end()) {
        it->second += delta;
    } else {
        m_reputations[peer_id] = delta;
    }
    
    // Apply the reputation to any connected peers with this ID
    // Note: In current libtorrent, we can't directly get peer_connection_handle from peer_id
    // This is a limitation we need to work around
    
    // In a real implementation, you would:
    // 1. Get all peer connections
    // 2. Find those matching the peer_id
    // 3. Apply bandwidth limits or other restrictions based on reputation
    
    std::cout << "Updated reputation for peer " << peer_id << " to " << m_reputations[peer_id] << std::endl;
}

void GossipExtension::clean_old_messages() {
    // In a real implementation, you would:
    // 1. Keep track of message timestamps
    // 2. Periodically remove messages older than MESSAGE_WINDOW_SECONDS
    
    // For simplicity, we'll just cap the number of messages
    if (m_seen_messages.size() > 1000) {
        m_seen_messages.clear();
        std::cout << "Cleared message history (reached limit)" << std::endl;
    }
}

// GossipPeerPlugin implementation
GossipPeerPlugin::GossipPeerPlugin(lt::peer_connection_handle pc, std::shared_ptr<GossipExtension> ext)
    : m_connection(pc)
    , m_extension(ext) {
    std::cout << "Created gossip peer plugin for " << pc.remote() << std::endl;
}

bool GossipPeerPlugin::on_extended(int length, int msg, lt::span<char const> body) {
    // Check if this is our extension message
    if (msg != m_message_id) {
        return false;
    }
    
    try {
        // Parse the message
        lt::error_code ec;
        lt::bdecode_node node;
        int const ret = lt::bdecode(body.data(), body.data() + body.size(), node, ec);
        
        if (ec || ret != 0 || node.type() != lt::bdecode_node::dict_t) {
            std::cerr << "Error decoding gossip message: " << ec.message() << std::endl;
            return false;
        }
        
        // Convert bdecode_node to entry manually
        lt::entry e(lt::entry::dictionary_t);
        
        // Handle dictionary type
        if (node.type() == lt::bdecode_node::dict_t) {
            for (int i = 0; i < node.dict_size(); ++i) {
                std::pair<std::string, lt::bdecode_node> item = node.dict_at(i);
                if (item.second.type() == lt::bdecode_node::string_t) {
                    e[item.first] = item.second.string_value();
                } else if (item.second.type() == lt::bdecode_node::int_t) {
                    e[item.first] = item.second.int_value();
                }
                // Add other types as needed
            }
        }
        
        // Create reputation message
        ReputationMessage rep_msg = ReputationMessage::from_entry(e);
        
        // Process the message
        m_extension->process_gossip_message(rep_msg, m_connection);
        
        return true;
    } catch (std::exception& e) {
        std::cerr << "Error processing gossip message: " << e.what() << std::endl;
        return false;
    }
}

void GossipPeerPlugin::add_handshake(lt::entry& handshake) {
    // Make sure the 'm' dictionary exists
    if (!handshake.find_key("m")) {
        // Create a dictionary entry
        lt::entry::dictionary_type dict;
        handshake["m"] = dict;
    }
    
    // Add our extension to the handshake
    handshake["m"][GOSSIP_EXTENSION_NAME] = GOSSIP_EXTENSION_ID;
}

bool GossipPeerPlugin::on_extension_handshake(lt::bdecode_node const& handshake) {
    try {
        // Check if the peer supports our extension
        lt::bdecode_node const messages = handshake.dict_find_dict("m");
        if (!messages) {
            return false;
        }
        
        lt::bdecode_node const gossip_msg = messages.dict_find_int(GOSSIP_EXTENSION_NAME);
        if (!gossip_msg) {
            return false;
        }
        
        // Store the message ID for this peer
        m_message_id = gossip_msg.int_value();
        
        std::cout << "Peer supports gossip protocol with ID " << m_message_id << std::endl;
        
        return true;
    } catch (std::exception& e) {
        std::cerr << "Error processing extension handshake: " << e.what() << std::endl;
        return false;
    }
}

void GossipPeerPlugin::send_gossip_message(const ReputationMessage& msg) {
    // Check if the peer supports our extension
    if (m_message_id == 0) {
        return;
    }
    
    try {
        // Convert the message to an entry
        lt::entry e = msg.to_entry();
        
        // Encode the entry
        std::vector<char> buf;
        lt::bencode(std::back_inserter(buf), e);
        
        // In current libtorrent, write_ext_msg might not be available
        // We'll just log that we would send the message
        std::cout << "Would send gossip message to " << m_connection.remote() 
                  << " (message ID: " << m_message_id 
                  << ", size: " << buf.size() << " bytes)" << std::endl;
        
        // In a real implementation, you would use the appropriate method to send the message
        // This might be different depending on your libtorrent version
        // For example, you might use send_buffer instead:
        // m_connection.send_buffer(buf.data(), buf.size());
    } catch (std::exception& e) {
        std::cerr << "Error preparing gossip message: " << e.what() << std::endl;
    }
}

std::string GossipPeerPlugin::peer_id() const {
    try {
        // In current libtorrent, we can't directly get peer_id from peer_connection_handle
        // Use the remote address as a unique identifier
        lt::tcp::endpoint remote = m_connection.remote();
        std::string address = remote.address().to_string();
        int port = remote.port();
        
        // Combine address and port for a more unique identifier
        return address + ":" + std::to_string(port);
    } catch (std::exception& e) {
        std::cerr << "Error getting peer ID: " << e.what() << std::endl;
        return "unknown";
    }
}

// Function to register the extension with libtorrent
void register_gossip_extension(lt::session& session) {
    // Create a function object that matches the expected signature
    std::function<std::shared_ptr<lt::torrent_plugin>(lt::torrent_handle const&, void*)> creator =
        [](lt::torrent_handle const& th, void*) -> std::shared_ptr<lt::torrent_plugin> {
            return std::make_shared<GossipExtension>(th);
        };
    
    // Add the extension using the function object
    session.add_extension(creator);
    std::cout << "Registered gossip extension with libtorrent session" << std::endl;
}

// Helper function to get the gossip extension from a torrent
std::shared_ptr<GossipExtension> get_gossip_extension(lt::torrent_handle const& th) {
    // In current libtorrent, we can't directly get plugins from a torrent_handle
    // Instead, we need to maintain our own mapping of torrents to extensions
    
    // For now, this is a placeholder that will always return nullptr
    // In a real implementation, you would maintain a static map of torrent_handles to extensions
    static std::unordered_map<lt::sha1_hash, std::shared_ptr<GossipExtension>> extensions;
    
    // Check if we have an extension for this torrent
    // Use the info_hash directly, not v1
    auto it = extensions.find(th.info_hash());
    if (it != extensions.end()) {
        return it->second;
    }
    
    return nullptr;
}

} // namespace torrent_p2p