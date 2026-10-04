#pragma once

#include <string>

namespace ChronosFlow {

class IBackpressureStrategy {
public:
    virtual ~IBackpressureStrategy() = default;
    virtual bool handle_saturation(size_t head, size_t tail, size_t capacity) = 0;
    virtual std::string get_name() const = 0;
};

// Discards oldest unread frames (optimal for real-time video/vision pipelines)
class DropOldestStrategy : public IBackpressureStrategy {
public:
    bool handle_saturation(size_t, size_t, size_t) override {
        return true; 
    }
    std::string get_name() const override { return "DropOldest (Real-Time Lossy)"; }
};

// Blocks the producer thread until consumer catches up (optimal for text/token streams)
class BlockProducerStrategy : public IBackpressureStrategy {
public:
    bool handle_saturation(size_t, size_t, size_t) override {
        return false;
    }
    std::string get_name() const override { return "BlockProducer (Lossless Strict)"; }
};

} // namespace ChronosFlow