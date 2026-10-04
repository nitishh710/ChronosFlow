#include "SharedMemoryRing.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>

using namespace ChronosFlow;

int main() {
    std::cout << "[Consumer] Connecting to Shared Memory Segment...\n";
    
    std::unique_ptr<SharedMemoryRing> ring;
    while (!ring) {
        try {
            ring = std::make_unique<SharedMemoryRing>("chronosflow_shm", false, std::make_unique<BlockProducerStrategy>());
        } catch (...) {
            std::cout << "[Consumer] Waiting for producer to initialize SHM...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    std::cout << "[Consumer] Attached! Ingesting shared tensor stream...\n";

    std::vector<float> local_tensor_buffer(512);
    TensorMetadata meta{};

size_t consumed_frames = 0;
    while (consumed_frames < 200) {
        if (ring->pop(meta, local_tensor_buffer.data())) {
            auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            double latency_us = static_cast<double>(now - meta.timestamp_ns) / 1000.0;

            if (consumed_frames % 30 == 0) {
                std::cout << "[Consumer] Ingested Frame: " << meta.frame_id 
                          << " | Transit Latency: " << latency_us << " us"
                          << " | Sample Value: " << local_tensor_buffer[0] << "\n";
            }
            consumed_frames++;
            
            // FIX: Break out of the loop if we just processed the final frame
            if (meta.frame_id == 199) {
                break;
            }
            
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

    std::cout << "[Consumer] Ingestion finished. Consumed: " << consumed_frames << " frames.\n";
    return 0;
}