#include "gossip.hpp"

namespace ba = boost::asio;
namespace bip = boost::asio::ip;

namespace torrent_p2p {

Gossip::Gossip(lt::session& session, int port): session_(session), port_(port), running_(false) {
    start();
}

void Gossip::start() {
    running_ = true;
    std::cout << "Starting gossip on port " << port_ << std::endl;
    
    // Create a work guard to keep io_context running
    // This prevents io_context.run() from returning when there's no work
    work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        io_context_.get_executor());
    
    // Start io_context thread
    service_thread_ = std::make_unique<std::thread>([this]() {
        try {
            std::cout << "IO context thread starting" << std::endl;
            io_context_.run();
            std::cout << "IO context thread exiting" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception in io_context thread: " << e.what() << std::endl;
        }
    });
    std::cout << "Started io_context thread" << std::endl;
    
    // Now start the acceptor
    try {
        std::cout << "Starting TCP acceptor on port " << port_ << std::endl;
        startAccept();
        std::cout << "TCP acceptor started successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to start acceptor: " << e.what() << std::endl;
    }
    
    // once a minute update the peer list to match the local dht
    peer_thread_ = std::make_unique<std::thread>([this]() {
        try {
            while (running_) {
                updateKnownPeers();
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in peer thread: " << e.what() << std::endl;
        }
    });
    std::cout << "Started peer thread" << std::endl;
    
    // send messages from outgoing queue
    send_thread_ = std::make_unique<std::thread>([this]() {
        try {
            while(running_) {
                processOutgoingMessages();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in send thread: " << e.what() << std::endl;
        }
    });
    std::cout << "Started send thread" << std::endl;
    
    // message processing thread
    message_processing_thread_ = std::make_unique<std::thread>([this]() {
        try {
            while (running_) {
                processIncomingMessages();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in message processing thread: " << e.what() << std::endl;
        }
    });
    std::cout << "Started message processing thread" << std::endl;
}

void Gossip::stop() {
    if (!running_) return;
    running_ = false;

    // First, reset the work guard to allow io_context to finish
    if (work_guard_) {
        std::cout << "Releasing work guard to allow io_context to exit" << std::endl;
        work_guard_.reset();
    }
    
    // Stop accepting new connections
    if (acceptor_ && acceptor_->is_open()) {
        std::cout << "Closing acceptor" << std::endl;
        boost::system::error_code ec;
        acceptor_->close(ec);
    }
    
    // Stop io_context (this is redundant once work_guard is reset, but keeping for safety)
    std::cout << "Stopping io_context" << std::endl;
    io_context_.stop();
    
    // Join threads
    if (service_thread_ && service_thread_->joinable()) {
        service_thread_->join();
    }
    
    if (message_processing_thread_ && message_processing_thread_->joinable()) {
        message_processing_thread_->join();
    }
    
    if (peer_thread_ && peer_thread_->joinable()) {
        peer_thread_->join();
    }
}

Gossip::~Gossip() {
    stop();
}

void Gossip::startAccept() {
    try {
        if(!acceptor_) {
            std::cout << "Creating new TCP acceptor on port " << port_ << std::endl;
            acceptor_ = std::make_unique<bip::tcp::acceptor>(
                io_context_,
                bip::tcp::endpoint(bip::tcp::v4(), port_)
            );
            
            // Set the acceptor to reuse the address (helps with quick restarts)
            acceptor_->set_option(bip::tcp::acceptor::reuse_address(true));
            
            // Check if acceptor is open
            if (!acceptor_->is_open()) {
                std::cerr << "ERROR: TCP acceptor failed to open on port " << port_ << std::endl;
                return;
            }
            
            std::cout << "TCP acceptor created and listening on port " << port_ << std::endl;
        }
        
        auto socket = std::make_shared<bip::tcp::socket>(io_context_);
        
        std::cout << "Waiting for incoming connections on port " << port_ << "..." << std::endl;
        
        // registering acceptor callback with io_context_, spawns a thread to handle this in background
        acceptor_->async_accept(*socket, [this, socket](const boost::system::error_code& error) {
            if (!error) {
                std::cout << "Accepted new connection from " << socket->remote_endpoint().address() 
                          << ":" << socket->remote_endpoint().port() << std::endl;
                handleAccept(socket);
            } else {
                std::cerr << "Gossip StartAccept: Failed to accept connection: " << error.message() << std::endl;
            }
            
            // continue accepting connections, create a new socket for each concurrent connection
            startAccept();
        });
    } catch (const std::exception& e) {
        std::cerr << "Exception in startAccept: " << e.what() << std::endl;
    }
}

// asynchronously reads messages from the socket and passes them to handleReceivedMessage()
void Gossip::handleAccept(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    // Get the endpoint of the sender
    lt::tcp::endpoint sender(
        socket->remote_endpoint().address(),
        socket->remote_endpoint().port()
    );
    std::cout << "Received Gossip from: " << sender << std::endl;
    auto size_buffer = std::make_shared<uint32_t>(0);

    // This async read call reads the first 4 bytes to get the message size
    boost::asio::async_read(*socket, 
        boost::asio::buffer(size_buffer.get(), sizeof(uint32_t)),
        [this, socket, sender, size_buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                std::cerr << "Error reading message size: " << error.message() << std::endl;
                return;
            }
            
            // This async read call uses message size to read the body of the message
            auto message_buffer = std::make_shared<std::vector<char>>(*size_buffer);
            boost::asio::async_read(*socket,
                boost::asio::buffer(message_buffer->data(), message_buffer->size()),
                [this, socket, sender, message_buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                    if (error) {
                        std::cerr << "Error reading message data: " << error.message() << std::endl;
                        return;
                    }
                    
                    // Process the received message
                    handleReceivedMessage(sender, *message_buffer);
                }
            );
        }
    );
    
}

// checks whether we've seen the message before, if we haven't spread the messege and add it it our incoming queue
void Gossip::handleReceivedMessage(const lt::tcp::endpoint& sender, const std::vector<char>& buffer) {
    try {
        GossipMessage message;
        if (!message.ParseFromArray(buffer.data(), buffer.size())) {
            std::cerr << "Failed to parse message" << std::endl;
            return;
        }

        if (inCache(message.message_id())) {
            std::cout << "Ignoring duplicate gossip message" << std::endl;
            return;
        }
        addToCache(message.message_id());

        IncomingMessage incoming;
        incoming.sender = sender;
        incoming.message = message;
        incoming.receive_time = std::time(nullptr);
        
        {
            std::lock_guard<std::mutex> lock(incoming_queue_mutex_);
            incoming_messages_.push(incoming);
        }
        spreadMessage(message, sender);
        
        std::cout << "Received and forwarded message from " << sender.address() << ":" << sender.port() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Gossip HandleReceivedMessage: " << e.what() << std::endl;
    }
}

void Gossip::sendMessageAsync(const lt::tcp::endpoint& target, const GossipMessage& msg) {
    try {
        // create socket
        auto socket = std::make_shared<bip::tcp::socket>(io_context_);

        // serialize message
        std::string serialized_msg;
        if(!msg.SerializeToString(&serialized_msg)) {
            std::cerr << "Gossip SendMessageAsync: " << "Failed to serialize message" << std::endl;
            return;
        }

        // get message size and add it to a buffer in front of the message string
        std::shared_ptr<std::vector<char>> buffer = std::make_shared<std::vector<char>>();
        uint32_t message_size = serialized_msg.size();

        buffer->resize(sizeof(message_size) + message_size);
        std::memcpy(buffer->data(), &message_size, sizeof(message_size));
        std::memcpy(buffer->data() + sizeof(message_size), serialized_msg.data(), message_size);
        
        // converting lt's tcp endpoint to asio's TCP endpoint
        bip::tcp::endpoint tcp_endpoint(target.address(),target.port());

        socket->async_connect(tcp_endpoint,
            [this, socket, buffer](const boost::system::error_code& error) {
                if (error) {
                    std::cerr << "Gossip SendMessageAsync: " << "Failed to connect to target" << std::endl;
                    return;
                }
                // write asynch
                ba::async_write(*socket, ba::buffer(*buffer),
                     [socket, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                        if (error) {
                            std::cerr << "Gossip SendMessageAsync: " << "Failed to write message" << std::endl;
                            return;
                        }
                        boost::system::error_code ec;
                        socket->shutdown(bip::tcp::socket::shutdown_both, ec);
                        socket->close(ec);
                    });
            });
    } catch (const std::exception& e) {
        std::cerr << "Gossip SendMessageAsync: " << e.what() << std::endl;
    }
    std::cout << "Message sent asynchronously to: " << target << std::endl;
}

// processes messages that are in the outgoing message queue
void Gossip::processOutgoingMessages() {
    std::vector<std::pair<lt::tcp::endpoint, GossipMessage>> messages_to_process;

    {
        std::lock_guard<std::mutex> lock(outgoing_queue_mutex_);

        const size_t max_batch_size = 10;
        size_t count = 0;

        while (!outgoing_messages_.empty() && count < max_batch_size) {
            messages_to_process.push_back(outgoing_messages_.front());
            outgoing_messages_.pop();
            count++;
        }

        for (auto& msg : messages_to_process) {
            sendMessageAsync(msg.first, msg.second);
        }

    }
}

// processes messages that have been accepted and added to incoming queue
// triggers callback functions in node classes
void Gossip::processIncomingMessages() {
    // Get a batch of messages from the queue
    std::vector<IncomingMessage> messages_to_process;
    
    // theses brackets create a limited scope for the std::lock_guard
    {
        // Lock the queue while we extract messages
        std::lock_guard<std::mutex> lock(incoming_queue_mutex_);
        
        // Get up to 10 messages at a time (to avoid processing too many at once)
        const size_t max_batch_size = 10;
        size_t count = 0;
        
        while (!incoming_messages_.empty() && count < max_batch_size) {
            messages_to_process.push_back(incoming_messages_.front());
            incoming_messages_.pop();
            count++;
        }
    }
    
    // Process each message
    for (const auto& incoming : messages_to_process) {
        const GossipMessage& message = incoming.message;
        const lt::tcp::endpoint& sender = incoming.sender;
        
        try {
            // Check which message type is set in the oneof field
            if (message.has_reputation()) {
                // Handle reputation message
                if (reputation_handler_) {
                    reputation_handler_(message.reputation(), sender);
                } else {
                    std::cerr << "Gossip: Received reputation message but no handler is registered" << std::endl;
                }
            }
            // Add handlers for other message types as they're added to the protocol
            // else if (message.has_content()) { ... }
            
            else {
                std::cerr << "Gossip: Received message with unknown type" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Gossip: Error processing message: " << e.what() << std::endl;
        }
    }
}

bool Gossip::inCache(const std::string& message_id) {
    std::lock_guard<std::mutex> lock(message_cache_mutex_);
    return message_cache_.find(message_id) != message_cache_.end();
}

void Gossip::addToCache(const std::string& message_id) {
    if (inCache(message_id)) return; // don't add duplicates to cache
    std::lock_guard<std::mutex> lock(message_cache_mutex_);
    message_cache_.insert(message_id);
    
    // Also add to LRU list for potential cleanup
    message_cache_list_.push_back({message_id, std::time(nullptr)});
    
    // Remove oldest entries if cache exceeds max size
    if (message_cache_list_.size() > max_cache_size) {
        auto oldest = message_cache_list_.front();
        message_cache_.erase(oldest.id);
        message_cache_list_.pop_front();
    }
}

std::string Gossip::generateMessageId(const GossipMessage& message) const {
    // Create a unique ID based on message content
    std::string id;
    
    // Include message type in ID
    if (message.has_reputation()) {
        const auto& rep = message.reputation();
        // Combine IP, port, timestamp, and reputation value
        id = rep.subject_ip() + ":" + std::to_string(rep.subject_port()) + ":" + 
             std::to_string(message.timestamp()) + ":" + 
             std::to_string(rep.reputation());
    }
    // Add other message types as needed
    
    return id;
}

void Gossip::updateKnownPeers() {
    auto peers = session_.session_state().dht_state.nodes;
    std::lock_guard<std::mutex> lock(peers_mutex_);
    // Add new peers to the list, avoiding duplicates
    for (const auto& peer : peers) {
        // Create a new endpoint with port + 1000 for gossip
        lt::tcp::endpoint gossip_peer(peer.address(), peer.port() + 1000);
        
        bool exists = false;
        for (const auto& known : known_peers_) {
            if (known.address() == gossip_peer.address() && known.port() == gossip_peer.port()) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            known_peers_.push_back(gossip_peer);
        }
    }
}

std::vector<lt::tcp::endpoint> Gossip::selectRandomPeers(size_t count, const lt::tcp::endpoint& exclude) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::vector<lt::tcp::endpoint> result;
    if(known_peers_.empty())
        return result;
    
    // Create a copy of peers excluding the sender
    std::vector<lt::tcp::endpoint> available_peers;
    for (const auto& peer : known_peers_) {
        if (peer.address() != exclude.address() || peer.port() != exclude.port()) {
            available_peers.push_back(peer);
        }
    }
    
    if (available_peers.empty())
        return result;
    
    // generating random seed
    std::random_device rd;
    std::mt19937 gen(rd());
    
    size_t select_count = std::min(count, available_peers.size());
    
    std::shuffle(available_peers.begin(), available_peers.end(), gen);
    result.assign(available_peers.begin(), available_peers.begin() + select_count);
    
    return result;
}

void Gossip::spreadMessage(const GossipMessage& message, const lt::tcp::endpoint& exclude) {
    std::cout << "spreading message" << std::endl;
    auto peers = selectRandomPeers(3, exclude);
    std::cout << "selected random peers" << std::endl;

    std::lock_guard<std::mutex> lock(outgoing_queue_mutex_);
    for (const auto & peer: peers) {
        std::cout << "peer: " << peer << std::endl;
        outgoing_messages_.push({peer, message});
    }
}

void Gossip::sendReputationMessage(const lt::tcp::endpoint& target, 
                                  const lt::tcp::endpoint& subject, 
                                  int reputation,
                                  const std::string& reason) {
    GossipMessage message = createGossipMessage(subject, reputation);
    outgoing_messages_.push({target, message});
}

GossipMessage Gossip::createGossipMessage(const lt::tcp::endpoint& subject, int reputation) const {
    GossipMessage message;
    message.set_timestamp(std::time(nullptr));
    
    ReputationMessage* rep_msg = message.mutable_reputation();
    rep_msg->set_subject_ip(subject.address().to_string());
    rep_msg->set_subject_port(subject.port());
    rep_msg->set_reputation(reputation);
    
    std::string id = generateMessageId(message);
    message.set_message_id(id);
    
    return message;
}

} // namespace