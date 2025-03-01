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
    explicit GossipExtension(lt::session &session);
    ~GossipExtension() override;
    
    void on_alert(lt::alert* alert) override;
    void on_tick() override;
    void on_piece_
private:
    
    lt::session &session_;
    // sets unchoke priority
    std::unordered_map<tcp::endpoint, uint64_t> peer_reputation_;
};

class GossipPeerPlugin: public lt::peer_plugin {
public:
    GossipPeerPlugin();
    ~GossipPeerPlugin() override;
}
}

#endif