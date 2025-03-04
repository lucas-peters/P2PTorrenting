#ifndef LAMPORT_HPP
#define LAMPORT_HPP

#include <mutex>
#include <cstdint>
#include <algorithm>

namespace torrent_p2p {

class LamportClock {
public:
    LamportClock();
    ~LamportClock();

    void incrementClock();
    void updateClock(uint64_t update_val);
    uint64_t getClock();
private:
    std::mutex clock_mutex_;
    uint64_t clock;
};

} // namespace torrent_p2p

#endif // LAMPORT_HPP