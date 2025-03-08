#include "lamport.hpp"

namespace torrent_p2p {

LamportClock::LamportClock(): clock(1) {}

LamportClock::~LamportClock() {}

void LamportClock::incrementClock() {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    clock++;
}

void LamportClock::updateClock(uint64_t update_val) {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    clock = std::max(clock, update_val) + 1;
}

uint64_t LamportClock::getClock() {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    return clock;
}

} // namespace torrent_p2p