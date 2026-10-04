#pragma once

#include "FutexMutex.hpp"
#include "TensorProtocol.hpp"
#include "BackpressureStrategy.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <atomic>
#include <memory>
#include <cstring>

namespace ChronosFlow {

constexpr size_t RING_CAPACITY = 8;

struct alignas(CACHE_LINE_SIZE) RingHeader {
    FutexLock lock;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_index{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_index{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> dropped_frames{0};
    SharedSlot slots[RING_CAPACITY];
};

class SharedMemoryRing {
private:
    std::string shm_name_;
    int shm_fd_{-1};
    RingHeader* header_{nullptr};
    bool is_creator_{false};
    std::unique_ptr<IBackpressureStrategy> strategy_;

public:
    SharedMemoryRing(const std::string& name, bool create, std::unique_ptr<IBackpressureStrategy> strategy)
        : shm_name_("/" + name), is_creator_(create), strategy_(std::move(strategy)) {
        
        size_t total_size = sizeof(RingHeader);

        if (is_creator_) {
            shm_unlink(shm_name_.c_str());
            shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
            if (shm_fd_ < 0) throw std::runtime_error("shm_open failed during creation");
            if (ftruncate(shm_fd_, total_size) != 0) throw std::runtime_error("ftruncate failed");
        } else {
            shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0666);
            if (shm_fd_ < 0) throw std::runtime_error("shm_open failed to connect to producer");
        }

        void* ptr = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (ptr == MAP_FAILED) throw std::runtime_error("mmap failed");

        header_ = static_cast<RingHeader*>(ptr);

        if (is_creator_) {
            new (&header_->lock) FutexLock();
            header_->write_index.store(0);
            header_->read_index.store(0);
            header_->dropped_frames.store(0);
        }
    }

    ~SharedMemoryRing() {
        if (header_) {
            munmap(header_, sizeof(RingHeader));
        }
        if (shm_fd_ >= 0) {
            close(shm_fd_);
        }
        if (is_creator_) {
            shm_unlink(shm_name_.c_str());
        }
    }

    bool push(const TensorMetadata& meta, const void* data) {
        if (meta.byte_size > TENSOR_PAYLOAD_SIZE) {
            throw std::length_error("Payload exceeds fixed slot size");
        }

        while (true) {
            size_t w = header_->write_index.load(std::memory_order_relaxed);
            size_t r = header_->read_index.load(std::memory_order_acquire);

            if ((w - r) >= RING_CAPACITY) {
                if (strategy_->handle_saturation(w, r, RING_CAPACITY)) {
                    header_->read_index.fetch_add(1, std::memory_order_release);
                    header_->dropped_frames.fetch_add(1, std::memory_order_relaxed);
                } else {
                    usleep(100);
                    continue;
                }
            }

            size_t slot_idx = w % RING_CAPACITY;
            SharedSlot& slot = header_->slots[slot_idx];

            std::memcpy(&slot.meta, &meta, sizeof(TensorMetadata));
            std::memcpy(slot.payload, data, meta.byte_size);

            header_->write_index.store(w + 1, std::memory_order_release);
            return true;
        }
    }

    bool pop(TensorMetadata& out_meta, void* out_data_dest) {
        size_t r = header_->read_index.load(std::memory_order_relaxed);
        size_t w = header_->write_index.load(std::memory_order_acquire);

        if (r >= w) {
            return false;
        }

        size_t slot_idx = r % RING_CAPACITY;
        SharedSlot& slot = header_->slots[slot_idx];

        std::memcpy(&out_meta, &slot.meta, sizeof(TensorMetadata));
        std::memcpy(out_data_dest, slot.payload, out_meta.byte_size);

        header_->read_index.store(r + 1, std::memory_order_release);
        return true;
    }

    size_t get_dropped_count() const {
        return header_->dropped_frames.load(std::memory_order_relaxed);
    }
};

} // namespace ChronosFlow