//
// Utility helpers for hashing RingBuffer500 via StateHash.
//

#ifndef UNTITLED_BEAMHASHHELPER_H
#define UNTITLED_BEAMHASHHELPER_H

#include "StateHash.h"
#include "RingBuffer500.h"

inline deque<int> ring_to_deque_const(const RingBuffer500 &buf) {
    deque<int> dq;
    for (int i = 0; i < buf.size(); i++) dq.push_back(buf[i]);
    return dq;
}

inline StateHash make_state_hash(const RingBuffer500 &A, const RingBuffer500 &B) {
    deque_hash ah(ring_to_deque_const(A));
    deque_hash bh(ring_to_deque_const(B));
    return StateHash(ah, bh);
}

inline uint64_t make_id_hash(uint64_t id) {
    uint64_t x = id;
    x += 0x9e3779b97f4a7c15;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
    x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
    return x ^ (x >> 31);
}

#endif //UNTITLED_BEAMHASHHELPER_H

