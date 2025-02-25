#ifndef GOSSIP_H
#define GOSSIP_H

#include <libtorrent/extensions.hpp> 
#include <libtorrent/torrent_handle.hpp>

#include "gossip.pb.h"

namespace torrent_p2p {

message GossipMessage {
    repeated PeerInfo peers = 1;
    message PeerInfo {
        string ip = 1;
        int port = 2;
    }
}

class GossipExtension : public lt::plugin {
public:
    void on_alert(lt::alert* alert) override;
    GossipExtension();
    ~GossipExtension();
private:
    lt::session &session_;
    // keeps a list of nodes we've connected to, if a new node shows up, prompt the dht to try adding it and send a gossip message to other nodes announcing the node
    std::unordered_map<lt::sha1_hash, int32_t> nodes_;
};

class GossipPeerPlugin: public lt::peer_plugin {
public:
    GossipPeerPlugin();
    ~GossipPeerPlugin() override;
}
}

#endif