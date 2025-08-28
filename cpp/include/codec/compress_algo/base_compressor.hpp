#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

namespace L2 {
namespace compress {

// Compression statistics for a single column
struct CompressionStats {
  size_t original_size_bytes = 0;    // Original data size in bytes
  size_t compressed_size_bytes = 0;  // Compressed data size in bytes
  size_t num_values = 0;             // Number of values compressed
  double compression_ratio = 0.0;    // compressed_size / original_size
  double space_saving_percent = 0.0; // (1 - compression_ratio) * 100
  std::string_view algorithm_name;   // Name of the algorithm used

  void calculate_metrics() {
    if (original_size_bytes > 0) {
      compression_ratio = static_cast<double>(compressed_size_bytes) / original_size_bytes;
      space_saving_percent = (1.0 - compression_ratio) * 100.0;
    }
  }
};

// Base interface for column compression algorithms
class BaseCompressor {
public:
  virtual ~BaseCompressor() = default;

  // Core compression interface
  virtual std::vector<uint8_t> compress(const void *data, size_t num_values, size_t value_size_bytes) = 0;
  virtual void decompress(const std::vector<uint8_t> &compressed_data, void *output, size_t num_values, size_t value_size_bytes) = 0;

  // Get algorithm name for stats
  virtual std::string_view get_algorithm_name() const = 0;

  // Get last compression stats
  const CompressionStats &get_stats() const { return stats_; }

protected:
  CompressionStats stats_;

  // Helper to update stats after compression
  void update_stats(size_t original_bytes, size_t compressed_bytes, size_t num_values) {
    stats_.original_size_bytes = original_bytes;
    stats_.compressed_size_bytes = compressed_bytes;
    stats_.num_values = num_values;
    stats_.algorithm_name = get_algorithm_name();
    stats_.calculate_metrics();
  }
};

// Factory function type for creating compressors
using CompressorFactory = std::unique_ptr<BaseCompressor> (*)();

// Utility functions for bit manipulation (extremely efficient for hot path)
namespace BitUtils {
// Calculate minimum bits needed to represent a value
inline constexpr uint8_t bits_needed(uint64_t max_value) {
  if (max_value == 0)
    return 1;
  uint8_t bits = 0;
  while (max_value > 0) {
    max_value >>= 1;
    ++bits;
  }
  return bits;
}

// Pack bits efficiently (hot path - inlined)
inline void pack_bits(const uint64_t *values, size_t count, uint8_t bits_per_value, std::vector<uint8_t> &output) {
  if (bits_per_value == 0 || count == 0)
    return;

  size_t total_bits = count * bits_per_value;
  size_t byte_count = (total_bits + 7) / 8;
  output.resize(byte_count, 0);

  size_t bit_pos = 0;
  for (size_t i = 0; i < count; ++i) {
    uint64_t value = values[i];
    for (uint8_t bit = 0; bit < bits_per_value; ++bit) {
      if (value & (1ULL << bit)) {
        size_t byte_idx = bit_pos / 8;
        size_t bit_in_byte = bit_pos % 8;
        output[byte_idx] |= (1 << bit_in_byte);
      }
      ++bit_pos;
    }
  }
}

// Unpack bits efficiently (extremely hot path - inlined)
inline void unpack_bits(const std::vector<uint8_t> &input, uint64_t *values, size_t count, uint8_t bits_per_value) {
  if (bits_per_value == 0 || count == 0)
    return;

  size_t bit_pos = 0;
  for (size_t i = 0; i < count; ++i) {
    uint64_t value = 0;
    for (uint8_t bit = 0; bit < bits_per_value; ++bit) {
      size_t byte_idx = bit_pos / 8;
      size_t bit_in_byte = bit_pos % 8;
      if (byte_idx < input.size() && (input[byte_idx] & (1 << bit_in_byte))) {
        value |= (1ULL << bit);
      }
      ++bit_pos;
    }
    values[i] = value;
  }
}

// Calculate 95th percentile value (for dynamic bitpack)
template <typename T>
inline T calculate_95th_percentile(const T *values, size_t count) {
  if (count == 0)
    return 0;
  if (count == 1)
    return values[0];

  // Simple approximation: sort and take 95th percentile
  std::vector<T> sorted_values(values, values + count);
  std::sort(sorted_values.begin(), sorted_values.end());

  size_t percentile_idx = static_cast<size_t>(count * 0.95);
  if (percentile_idx >= count)
    percentile_idx = count - 1;

  return sorted_values[percentile_idx];
}
} // namespace BitUtils

// Delta encoding utilities (for columns with use_delta=true)
namespace DeltaUtils {
// Apply delta encoding to array (modifies in place for efficiency)
template <typename T>
inline void encode_deltas(T *values, size_t count) {
  if (count <= 1)
    return;

  for (size_t i = count - 1; i > 0; --i) {
    values[i] = values[i] - values[i - 1];
  }
  // values[0] remains unchanged as the base value
}

// Decode delta encoding (modifies in place for efficiency)
template <typename T>
inline void decode_deltas(T *values, size_t count) {
  if (count <= 1)
    return;

  for (size_t i = 1; i < count; ++i) {
    values[i] = values[i] + values[i - 1];
  }
}

// Check if delta encoding would be beneficial (heuristic)
template <typename T>
inline bool should_use_delta(const T *values, size_t count) {
  if (count < 2)
    return false;

  // Calculate variance of original vs delta-encoded values
  uint64_t original_max = 0, delta_max = 0;

  for (size_t i = 0; i < count; ++i) {
    if (values[i] > original_max)
      original_max = values[i];
  }

  if (count > 1) {
    for (size_t i = 1; i < count; ++i) {
      uint64_t delta = std::abs(static_cast<int64_t>(values[i]) - static_cast<int64_t>(values[i - 1]));
      if (delta > delta_max)
        delta_max = delta;
    }
  }

  // Use delta if delta representation needs fewer bits
  return BitUtils::bits_needed(delta_max) < BitUtils::bits_needed(original_max);
}
} // namespace DeltaUtils

} // namespace compress
} // namespace L2
