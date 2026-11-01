#pragma once

// Fixed-size single-producer / single-consumer queue that counts what it
// drops. Hardware-free: the lock is a template parameter, so the embedded
// build guards it with a FreeRTOS critical section and tests use NoLock.
//
// Holds exactly Capacity items. Slots are filled in place under the lock, so a
// large item is never built on the producer's stack.

#include <stddef.h>
#include <stdint.h>

namespace Scarfnet {

struct NoLock {
    void lock()   {}
    void unlock() {}
};

template <typename T, size_t Capacity, typename Lock = NoLock>
class RxQueue
{
public:
    // Calls fill(T&) on the next free slot. Returns false, and counts a drop,
    // if the queue is full.
    template <typename Fill>
    bool push(Fill&& fill)
    {
        _lock.lock();
        bool ok = _count < Capacity;
        if (ok) {
            fill(_slots[(_head + _count) % Capacity]);
            ++_count;
        } else {
            ++_dropped;
        }
        _lock.unlock();
        return ok;
    }

    bool pop(T& out)
    {
        _lock.lock();
        bool ok = _count > 0;
        if (ok) {
            out   = _slots[_head];
            _head = (_head + 1) % Capacity;
            --_count;
        }
        _lock.unlock();
        return ok;
    }

    // Drops since the last call.
    uint32_t takeDropped()
    {
        _lock.lock();
        uint32_t dropped = _dropped;
        _dropped = 0;
        _lock.unlock();
        return dropped;
    }

    Lock& lock() { return _lock; }

private:
    T        _slots[Capacity];
    size_t   _head    = 0;
    size_t   _count   = 0;
    uint32_t _dropped = 0;
    Lock     _lock;
};

} // namespace Scarfnet
