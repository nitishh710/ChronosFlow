#pragma once

#include <cstdint>
#include <cstddef>

namespace ChronosFlow {

constexpr size_t MAX_DIMENSIONS = 4;
constexpr size_t TENSOR_PAYLOAD_SIZE = 1024 * 1024; // 1MB payload buffer per slot
constexpr size_t CACHE_LINE_SIZE = 64;

enum class DataType : uint32_t {
    FLOAT32 = 0,
    INT8    = 1,
    FLOAT16 = 2
};

struct TensorMetadata {
    uint64_t frame_id;
    uint64_t timestamp_ns;
    DataType dtype;
    uint32_t ndim;
    uint32_t shape[MAX_DIMENSIONS];
    size_t byte_size;
};

// 64-byte cache-line aligned to eliminate false sharing between CPU cores
struct alignas(CACHE_LINE_SIZE) SharedSlot {
    TensorMetadata meta;
    uint8_t payload[TENSOR_PAYLOAD_SIZE];
};

} // namespace ChronosFlow