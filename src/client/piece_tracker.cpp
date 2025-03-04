#include <unordered_map>
#include <set>
#include <vector>

struct PieceTracker {
    std::unordered_map<lt::piece_index_t, std::unordered_map<lt::tcp::endpoint, std::set<int>>> piece_contributors;

    void add_block(lt::piece_index_t piece, int block, lt::tcp::endpoint const& peer) {
        piece_constributrs[piece][peer].insert(block);
    }

    std::vector<lt::tcp::endpoint> get_piece_contributors(lt::piece_index_t piece) {
        std::vector<lt::tcp::endpoint> peer_return;
        for (auto& peer : piece_contributors[piece]) {
            peer_return.push_back(peer.first);
        }
        return peer_return;
    }

    void clear_piece(lt::piece_index_t piece) {
        piece_contributors.erase(piece);
    }
};