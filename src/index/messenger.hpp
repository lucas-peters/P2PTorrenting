#ifndef MESSENGER_H
#define MESSENGER_H

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

#include "index.pb.h"
#include "gossip/lamport.hpp"

namespace ba = boost::asio;
namespace bip = boost::asio::ip;

namespace torrent_p2p {

// Custom comparator for priority queue to compare messages based on Lamport timestamp
struct MessageComparator {
    bool operator()(
        const std::pair<lt::tcp::endpoint, IndexMessage>& a,
        const std::pair<lt::tcp::endpoint, IndexMessage>& b
    ) const {
        // Higher timestamp = lower priority (we want oldest messages first)
        return a.second.lamport_timestamp() > b.second.lamport_timestamp();
    }
};

// Messenger is for one-to-one messages between client and index
class Messenger {

public:
    Messenger(lt::session& session, int port);
    ~Messenger() { stop(); }
    
    using IndexHandler = std::function<void(const IndexMessage&, const lt::tcp::endpoint&)>;
    void setIndexHandler(IndexHandler handler) {index_handler_ = handler;}
    
    void queueMessage(const lt::tcp::endpoint& target, const IndexMessage& msg);
    
    int getPort() const;

private:
    bool running_ = false;
    int port_;
    lt::session& session_;

    void start();
    void stop();
    
    // Message handler
    IndexHandler index_handler_;
    
    // Boost::Asio
    boost::asio::io_context io_context_;
    std::unique_ptr<ba::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::unique_ptr<bip::tcp::acceptor> acceptor_;
    
    // Threads
    std::unique_ptr<std::thread> service_thread_;
    std::unique_ptr<std::thread> send_thread_;
    std::unique_ptr<std::thread> receive_thread_;
    
    // Queues
    std::queue<std::pair<lt::tcp::endpoint, IndexMessage>> send_queue_;
    std::priority_queue<
        std::pair<lt::tcp::endpoint, IndexMessage>,
        std::vector<std::pair<lt::tcp::endpoint, IndexMessage>>,
        MessageComparator
    > receive_queue_;
    
    // Mutexes
    std::mutex send_mutex_;
    std::mutex receive_mutex_;
    
    LamportClock lamport_clock_;
    
    // Helper methods
    void startAccept();
    void handleAccept(std::shared_ptr<bip::tcp::socket> socket);
    void handleReceivedMessage(const lt::tcp::endpoint& sender, const std::vector<char>& buffer);
    void processOutgoingMessages();
    void processIncomingMessages();
    void sendMessageAsync(const lt::tcp::endpoint& target, const IndexMessage& msg);
};

} // namespace
#endif