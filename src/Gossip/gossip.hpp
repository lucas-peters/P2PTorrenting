#ifndef GOSSIP_H
#define GOSSIP_H

#include <libtorrent/config.hpp>
#include <libtorrent/extensions.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/socket.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/peer_connection_handle.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/bencode.hpp>

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <chrono>
#include <random>
#include <functional>

namespace torrent_p2p {

// Extension name constant
static constexpr char GOSSIP_EXTENSION_NAME[] = "gossip_protocol";
static constexpr int GOSSIP_EXTENSION_ID = 1; // Custom extension ID

// Message structure for gossip protocol
struct ReputationMessage {
    std::string peer_id;            // ID of the peer being rated
    std::int64_t reputation_delta;  // Change in reputation (positive or negative)
    std::uint32_t message_id;       // Unique ID for this message to prevent duplicates
    std::int64_t timestamp;         // When the message was created
    std::string source_id;          // ID of the node that created this message
    int ttl = 10;                   // Time-to-live for message propagation
    
    // Serialize to entry for sending over the wire
    lt::entry to_entry() const;
    
    // Deserialize from entry received from the wire
    static ReputationMessage from_entry(const lt::entry& e);
};

// Forward declarations
class GossipPeerPlugin;

// The main extension class that implements the gossip protocol
class GossipExtension : public lt::torrent_plugin, public std::enable_shared_from_this<GossipExtension> {
public:
    // Constructor
    GossipExtension(lt::torrent_handle th);
    
    // Destructor
    ~GossipExtension();
    
    // Create a new peer plugin for each peer connection
    std::shared_ptr<lt::peer_plugin> new_connection(lt::peer_connection_handle const& pc) override;
    
    // Process a received gossip message
    bool process_gossip_message(const ReputationMessage& msg, const lt::peer_connection_handle& source);
    
    // Create a new reputation message and propagate it
    void create_reputation_update(const std::string& peer_id, std::int64_t delta);
    
    // Get the current reputation for a peer
    std::int64_t get_peer_reputation(const std::string& peer_id) const;
    
    // Factory function to create the plugin
    static std::shared_ptr<lt::torrent_plugin> create(lt::torrent_handle const& th, void*);

private:
    // The torrent this extension is attached to
    lt::torrent_handle m_torrent;
    
    // Mutex for thread safety
    mutable std::mutex m_mutex;
    
    // Map of peer IDs to reputation values
    std::unordered_map<std::string, std::int64_t> m_reputations;
    
    // Set of message IDs we've already seen (to prevent duplicates)
    std::unordered_set<std::uint32_t> m_seen_messages;
    
    // Random number generator for selecting peers
    mutable std::mt19937 m_rng;
    
    // Maximum number of peers to forward a message to
    static constexpr int MAX_GOSSIP_PEERS = 3;
    
    // Time window for tracking seen messages (in seconds)
    static constexpr int MESSAGE_WINDOW_SECONDS = 3600; // 1 hour
    
    // Helper methods
    void propagate_message(const ReputationMessage& msg, const lt::peer_connection_handle& source);
    std::vector<lt::peer_connection_handle> select_random_peers(int count, const lt::peer_connection_handle& exclude);
    void update_peer_reputation(const std::string& peer_id, std::int64_t delta);
    void clean_old_messages();
};

// Plugin for individual peer connections
class GossipPeerPlugin : public lt::peer_plugin {
public:
    // Constructor
    GossipPeerPlugin(lt::peer_connection_handle pc, std::shared_ptr<GossipExtension> ext);
    
    // Called when we receive a custom extension message
    bool on_extended(int length, int msg, lt::span<char const> body) override;
    
    // Add the gossip extension to the handshake
    void add_handshake(lt::entry& handshake) override;
    
    // Called when we receive a handshake from a peer
    bool on_extension_handshake(lt::bdecode_node const& handshake) override;
    
    // Send a gossip message to this peer
    void send_gossip_message(const ReputationMessage& msg);
    
    // Get the peer's ID
    std::string peer_id() const;

private:
    // The peer connection this plugin is attached to
    lt::peer_connection_handle m_connection;
    
    // Reference to the parent extension
    std::shared_ptr<GossipExtension> m_extension;
    
    // Extension message ID (assigned during handshake)
    int m_message_id = 0;
};

// Function to register the gossip extension with libtorrent
void register_gossip_extension(lt::session& session);

// Helper function to get the gossip extension from a torrent
std::shared_ptr<GossipExtension> get_gossip_extension(lt::torrent_handle const& th);

} // namespace torrent_p2p

#endif // GOSSIP_H