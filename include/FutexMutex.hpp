#pragma once

#include <atomic>
#include <cstdint>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <cerrno>

namespace ChronosFlow {

class FutexLock {
private:
    // 0: Unlocked, 1: Locked without waiters, 2: Locked with waiters
    std::atomic<uint32_t> state_{0};

    static int futex(uint32_t* uaddr, int op, uint32_t val, const struct timespec* timeout) {
        return syscall(SYS_futex, uaddr, op, val, timeout, nullptr, 0);
    }

public:
    FutexLock() = default;
    FutexLock(const FutexLock&) = delete;
    FutexLock& operator=(const FutexLock&) = delete;

    void lock() {
        uint32_t expected = 0;
        // Fast path: Atomic CAS in user space without entering the kernel
        if (state_.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
            return;
        }

        // Contention path: Flag waiters and sleep via Linux futex syscall
        if (expected != 2) {
            expected = state_.exchange(2, std::memory_order_acquire);
        }
        while (expected != 0) {
            futex(reinterpret_cast<uint32_t*>(&state_), FUTEX_WAIT, 2, nullptr);
            expected = state_.exchange(2, std::memory_order_acquire);
        }
    }

    void unlock() {
        // Fast path: If zero waiters are pending, release purely in user space
        if (state_.fetch_sub(1, std::memory_order_release) != 1) {
            state_.store(0, std::memory_order_release);
            futex(reinterpret_cast<uint32_t*>(&state_), FUTEX_WAKE, 1, nullptr);
        }
    }
};

class ScopedFutexGuard {
private:
    FutexLock& lock_;
public:
    explicit ScopedFutexGuard(FutexLock& lock) : lock_(lock) { lock_.lock(); }
    ~ScopedFutexGuard() { lock_.unlock(); }
};

} // namespace ChronosFlow