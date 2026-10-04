#include "SharedMemoryRing.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>

using namespace ChronosFlow;

int main() {
    std::cout << "[Producer] Initializing Shared Memory Ring Buffer...\n";
    auto strategy = std::make_unique<DropOldestStrategy>();
    SharedMemoryRing ring("chronosflow_shm", true, std::move(strategy));

    std::cout << "[Producer] Ready. Streaming 512-dim visual embeddings @ 60 FPS...\n";

    constexpr size_t EMBEDDING_DIM = 512;
    std::vector<float> mock_embedding(EMBEDDING_DIM, 0.0f);

    for (uint64_t frame_id = 0; frame_id < 200; ++frame_id) {
        for (size_t i = 0; i < EMBEDDING_DIM; ++i) {
            mock_embedding[i] = static_cast<float>(frame_id * 0.01f + i);
        }

        TensorMetadata meta{};
        meta.frame_id = frame_id;
        meta.dtype = DataType::FLOAT32;
        meta.ndim = 2;
        meta.shape[0] = 1;
        meta.shape[1] = EMBEDDING_DIM;
        meta.byte_size = EMBEDDING_DIM * sizeof(float);
        meta.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        ring.push(meta, mock_embedding.data());

        if (frame_id % 30 == 0) {
            std::cout << "[Producer] Pushed Frame: " << frame_id 
                      << " | Bytes: " << meta.byte_size 
                      << " | Dropped upstream: " << ring.get_dropped_count() << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    std::cout << "[Producer] Run complete. Total dropped frames: " << ring.get_dropped_count() << "\n";
    std::cout << "[Producer] Holding SHM alive for consumer termination...\n";
    std::this_thread::sleep_for(std::chrono::seconds(4));
    return 0;
}