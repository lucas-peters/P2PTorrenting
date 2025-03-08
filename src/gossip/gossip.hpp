#ifndef GOSSIP_H
#define GOSSIP_H

#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <list>
#include <random>
#include <algorithm>
#include <queue>

#include <libtorrent/session.hpp>

#include "gossip.pb.h"
#include "lamport.hpp"

namespace torrent_p2p {
    
// Forward declaration
class Client;

class Gossip {
public:
    // Constructor takes a reference to the libtorrent session
    Gossip(lt::session& session, int port = 6789);
    // Constructor with explicit host IP
    Gossip(lt::session& session, int port, const std::string& host_ip);
    ~Gossip();

    void start();
    void stop();
    
    // Send a gossip message to a specific peer
    void sendGossipMessage(const lt::tcp::endpoint& target, const GossipMessage& message);
    
    // Send a reputation message (convenience method)
    void sendReputationMessage(const lt::tcp::endpoint& target, const lt::tcp::endpoint& subject,
                              int reputation, const std::string& reason = "");

    // message spreading
    void spreadMessage(const GossipMessage& message, const lt::tcp::endpoint& exclude);
    std::vector<lt::tcp::endpoint> selectRandomPeers(size_t count, const lt::tcp::endpoint& exclude);
    GossipMessage createGossipMessage(const lt::tcp::endpoint& subject, int reputation) const;
    
    // Setting handlers for reputation messages
    using ReputationHandler = std::function<void(const ReputationMessage&, const lt::tcp::endpoint&)>;
    //using IndexHandler = std::function<void(const IndexMessage&, const lt::tcp::endpoint&)>;
    void setReputationHandler(ReputationHandler handler) { reputation_handler_ = handler; };
    //void setIndexHandler(IndexHandler handler) {index_handler_ = handler};

    std::function<void(const lt::tcp::endpoint&)> heartbeat_handler_;

    void setHeartbeatHandler(std::function<void(const lt::tcp::endpoint&)> handler) {heartbeat_handler_ = handler;}

    void sendMessage(const lt::tcp::endpoint& target, const GossipMessage& message) {
        std::lock_guard<std::mutex> lock(outgoing_queue_mutex_);
        outgoing_messages_.push({target, message});
    }

private:
    bool running_ = false;
    int port_;
    lt::session& session_;
    std::string host_ip_ = "127.0.0.1"; // Default to localhost if not specified

    // threads
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<std::thread> service_thread_;
    std::unique_ptr<std::thread> peer_thread_;
    std::unique_ptr<std::thread> send_thread_;
    std::unique_ptr<std::thread> message_processing_thread_;

    // Work guard to keep io_context running
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    
    // Message handlers - can add more to support multiple types of messages
    ReputationHandler reputation_handler_;
    // This handler is used for index Nodes to gossip to each other about added torrents
    //IndexHandler index_handler_;
    
    // struct to help incoming message queue order messages by lamport timestamp
    struct IncomingMessage {
        lt::tcp::endpoint sender;
        GossipMessage message;
        std::time_t receive_time;

        // overloading < for priority queue
        bool operator<(const IncomingMessage& other) const {
            return message.lamport_timestamp() > other.message.lamport_timestamp();
        }
    };

    // Queues, incoming and outgoing
    std::priority_queue<IncomingMessage> incoming_messages_;
    std::queue<std::pair<lt::tcp::endpoint, GossipMessage>> outgoing_messages_;
    std::mutex outgoing_queue_mutex_;
    std::mutex incoming_queue_mutex_;
    LamportClock lamport_clock_;

    // Asynchronous message receiving
    void processIncomingMessages();
    void startAccept();
    void handleAccept(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
    void handleReceivedMessage(const lt::tcp::endpoint& sender, const std::vector<char>& buffer);
    void updateKnownPeers();

    
    // Asynchronous message sending
    void processOutgoingMessages();
    void sendMessageAsync(const lt::tcp::endpoint& target, const GossipMessage& message);
    void handleConnect(const boost::system::error_code& error, 
                      boost::asio::ip::tcp::socket* socket, 
                      std::shared_ptr<std::string> serialized_message);
    void handleWrite(const boost::system::error_code& error, 
                    size_t bytes_transferred, 
                    boost::asio::ip::tcp::socket* socket, 
                    std::shared_ptr<std::string> serialized_message);

    // lru cache of messages
    struct MessageCacheEntry {
        std::string id;
        std::time_t timestamp;
    };

    // Cache
    std::list<MessageCacheEntry> message_cache_list_;
    std::unordered_set<std::string> message_cache_;
    std::mutex message_cache_mutex_;
    size_t max_cache_size = 1000;
    bool inCache(const std::string& message_id);
    void addToCache(const std::string& message_id);

    // Known peers for message propagation
    std::vector<lt::tcp::endpoint> known_peers_;
    std::mutex peers_mutex_;
    std::string generateMessageId(const GossipMessage& message) const;
};

} // namespace torrent_p2p

#endif