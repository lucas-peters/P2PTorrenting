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
#include "lamport.hpp"

namespace torrent_p2p {

class Messenger {

public:
    Messenger(int port);
    ~Messenger();

    void sendMessageAsync(const T& message, const lt::tcp::endpoint& target);
    void receiveMessageAsync();

    using IndexHandler = std::function<void(const IndexMessage&, const lt::tcp::endpoint&)>;
    void setIndexHandler(IndexHandler handler) {index_handler_ = handler;}

private:
    bool running_ = false;
    int port_;
    IndexHandler index_handler_;
    // I don't know if I need a session
    lt::session& session_;

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<std::thread> service_thread_;
    std::unique_ptr<std::thread> send_thread_;

    std::queue<std::pair<lt::tcp::endpoint, IndexMessage>> send_queue_;
    std::priority_queue<std::pair<lt::tcp::endpoint, IndexMessage>> receive_queue_;

    std::mutex send_queue_mutex_;
    std::mutex receive_queue_mutex_;

    LamportClock lamport_clock_;
};

} // namespace
#endif