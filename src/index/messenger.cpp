#include "messenger.hpp"

// Most of the functions here are reimplementations or copies from our gossip class
// should consider making them inherit, but I don't want to break anything rn
namespace torrent_p2p {

Messenger::Messenger(lt::session& session, int port, std::string ip) : session_(session), port_(port), ip_(ip) {
    start();   
}

void Messenger::start() {
    running_ = true;

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
        startAccept();
        std::cout << "TCP acceptor started successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to start acceptor: " << e.what() << std::endl;
    }
    
    // send messages from send_queue_
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
    receive_thread_ = std::make_unique<std::thread>([this]() {
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

void Messenger::stop() {
    if (!running_) return;
    running_ = false;

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
    
    if (send_thread_ && send_thread_->joinable()) {
        send_thread_->join();
    }

    if (receive_thread_ && receive_thread_->joinable()) {
        receive_thread_->join();
    }
}

// Accepts Incoming Connection on a socket
void Messenger::startAccept() {
    try {
        if(!acceptor_ || !acceptor_->is_open()) {
            std::cerr << "ERROR: Cannot start accepting connections - acceptor is not initialized or not open" << std::endl;
            return;
        }
        
        auto socket = std::make_shared<bip::tcp::socket>(io_context_);
        
        std::cout << "Waiting for incoming connections on port " << port_ << "..." << std::endl;
        
        // registering acceptor callback with io_context_, spawns a thread to handle this in background
        acceptor_->async_accept(*socket, [this, socket](const boost::system::error_code& error) {
            if (!error) {
                std::cout << "Accepted new connection from " << socket->remote_endpoint().address() 
                          << ":" << socket->remote_endpoint().port() << std::endl;
                // This function deals with logic once a message has been accepted on the socket
                handleAccept(socket);
            } else {
                std::cerr << "Messenger StartAccept: Failed to accept connection: " << error.message() << std::endl;
            }
            
            // continue accepting connections, create a new socket for each concurrent connection
            startAccept();
        });
    } catch (const std::exception& e) {
        std::cerr << "Exception in startAccept: " << e.what() << std::endl;
    }
}

// asynchronously reads messages from the socket and passes them to handleReceivedMessage()
void Messenger::handleAccept(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    // ee the endpoint of the sender
    lt::tcp::endpoint sender(
        socket->remote_endpoint().address(),
        socket->remote_endpoint().port()
    );
    std::cout << "Received Message Request From:  " << sender << std::endl;
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
void Messenger::handleReceivedMessage(const lt::tcp::endpoint& sender, const std::vector<char>& buffer) {
    try {
        IndexMessage message;
        if (!message.ParseFromArray(buffer.data(), buffer.size())) {
            std::cerr << "Failed to parse message" << std::endl;
            return;
        }
        
        {
            std::lock_guard<std::mutex> lock(receive_mutex_);
            receive_queue_.push({sender, message});
        }
        
        std::cout << "Received message from " << sender.address() << ":" << sender.port() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ERROR Messenger HandleReceivedMessage: " << e.what() << std::endl;
    }
}

void Messenger::sendMessageAsync(const lt::tcp::endpoint& target, const IndexMessage& msg) {
    try {
        // create socket
        auto socket = std::make_shared<bip::tcp::socket>(io_context_);

        // serialize message
        std::string serialized_msg;
        if(!msg.SerializeToString(&serialized_msg)) {
            std::cerr << "Index SendMessageAsync: " << "Failed to serialize message" << std::endl;
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
                    std::cerr << "Index SendMessageAsync: " << "Failed to connect to target" << std::endl;
                    return;
                }
                // write asynch
                ba::async_write(*socket, ba::buffer(*buffer),
                     [socket, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                        if (error) {
                            std::cerr << "Index SendMessageAsync: " << "Failed to write message" << std::endl;
                            return;
                        }
                        boost::system::error_code ec;
                        socket->shutdown(bip::tcp::socket::shutdown_both, ec);
                        socket->close(ec);
                    });
            });
    } catch (const std::exception& e) {
        std::cerr << "Index SendMessageAsync: " << e.what() << std::endl;
    }
    std::cout << "Index message sent asynchronously to: " << target << std::endl;
}

// Queue a message to be sent asynchronously
void Messenger::queueMessage(const lt::tcp::endpoint& target, const IndexMessage& msg) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    send_queue_.push(std::make_pair(target, msg));
}

// processes messages that are in the outgoing message queue
void Messenger::processOutgoingMessages() {
    std::vector<std::pair<lt::tcp::endpoint, IndexMessage>> messages_to_process;

    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        const size_t max_batch_size = 10;
        size_t count = 0;

        while (!send_queue_.empty() && count < max_batch_size) {
            messages_to_process.push_back(send_queue_.front());
            send_queue_.pop();
            count++;
        }
    }

    for (auto& msg : messages_to_process) {
        // attaching lamport clock timestamp to outgoing message
        msg.second.set_source_ip(ip_);
        msg.second.set_source_port(port_);
        msg.second.set_lamport_timestamp(lamport_clock_.getClock());
        lamport_clock_.incrementClock();
        sendMessageAsync(msg.first, msg.second);
    }
}

// processes messages that have been accepted and added to incoming queue
// triggers callback functions in node classes
void Messenger::processIncomingMessages() {
    // Get a batch of messages from the queue
    std::vector<std::pair<lt::tcp::endpoint, IndexMessage>> messages_to_process;
    
    // theses brackets create a limited scope for the std::lock_guard
    {
        // Lock the queue while we extract messages
        std::lock_guard<std::mutex> lock(receive_mutex_);
        
        // Get up to 10 messages at a time (to avoid processing too many at once)
        const size_t max_batch_size = 10;
        size_t count = 0;
        
        while (!receive_queue_.empty() && count < max_batch_size) {
            messages_to_process.push_back(receive_queue_.top()); // gets message with lowest lamport timestamp
            receive_queue_.pop();
            count++;
        }
    }
    
    // Process each message
    for (const auto& incoming : messages_to_process) {
        const IndexMessage& message = incoming.second;
        const lt::tcp::endpoint sender(lt::make_address_v4(message.source_ip()), message.source_port());
        
        try {
            // updating lamport clock
            if (message.lamport_timestamp() > 0) {
                lamport_clock_.updateClock(message.lamport_timestamp());
            }
            index_handler_(sender, message);
            // Check which message type is set in the oneof field
            // if (message.has_reputation()) {
            //     // Handle reputation message
            //     if (reputation_handler_) {
            //         reputation_handler_(message.reputation(), sender);
            //     } else {
            //         std::cerr << "Gossip: Received reputation message but no handler is registered" << std::endl;
            //     }
            // }
            // Add handlers for other message types as they're added to the protocol
            // else if (message.has_content()) { ... }
            
            // else {
            //     std::cerr << "Messenger: Received message with unknown type" << std::endl;
            // }
        } catch (const std::exception& e) {
            std::cerr << "Messenger: Error processing message: " << e.what() << std::endl;
        }
    }
}

} // namespace
