#include <libtorrent/extensions.hpp> 
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/alert_types.hpp>
#include "gossip.hpp"

namespace torrent_p2p {

GossipExtension::GossipExtension() {}

void GossipExtension::on_alert(lt::alert* alert) {
    if (auto* peer_alert = lt::alert_cast<lt::dht_live_nodes_alert>(alert)) {
        // Construct a gossip message to send to other nodes to alert them of a new peer joining the network
    }
}
}