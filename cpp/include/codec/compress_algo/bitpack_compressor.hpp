#pragma once

#include "base_compressor.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace L2 {
namespace compress {

// BitPack Static compressor
// Uses fixed bit width from schema for all values
class BitPackStaticCompressor : public BaseCompressor {
public:
  explicit BitPackStaticCompressor(uint8_t bit_width) : bit_width_(bit_width) {}

  std::vector<uint8_t> compress(const void *data, size_t num_values, size_t value_size_bytes) override {
    std::vector<uint8_t> result;
    if (num_values == 0 || bit_width_ == 0) {
      update_stats(0, 0, 0);
      return result;
    }

    size_t original_size = num_values * value_size_bytes;

    // Convert input to uint64_t array for bit packing
    std::vector<uint64_t> values(num_values);
    convert_to_uint64(data, values.data(), num_values, value_size_bytes);

    // Header: num_values, value_size_bytes, bit_width
    size_t header_size = sizeof(size_t) + sizeof(size_t) + sizeof(uint8_t);

    // Calculate packed size
    size_t total_bits = num_values * bit_width_;
    size_t packed_bytes = (total_bits + 7) / 8;

    result.resize(header_size + packed_bytes);
    size_t pos = 0;

    // Write header
    write_value(result, pos, num_values);
    write_value(result, pos, value_size_bytes);
    write_value(result, pos, bit_width_);

    // Pack bits
    std::vector<uint8_t> packed_data;
    BitUtils::pack_bits(values.data(), num_values, bit_width_, packed_data);

    std::memcpy(&result[pos], packed_data.data(), packed_data.size());

    update_stats(original_size, result.size(), num_values);
    return result;
  }

  void decompress(const std::vector<uint8_t> &compressed_data, void *output, size_t num_values, size_t value_size_bytes) override {
    if (compressed_data.size() < sizeof(size_t) + sizeof(size_t) + sizeof(uint8_t))
      return;

    size_t pos = 0;

    // Read header
    uint8_t stored_bit_width = read_value<uint8_t>(compressed_data, pos);

    assert(stored_num_values == num_values && stored_value_size == value_size_bytes);

    // Extract packed data
    std::vector<uint8_t> packed_data(compressed_data.begin() + pos, compressed_data.end());

    // Unpack bits
    std::vector<uint64_t> values(num_values);
    BitUtils::unpack_bits(packed_data, values.data(), num_values, stored_bit_width);

    // Convert back to original format
    convert_from_uint64(values.data(), output, num_values, value_size_bytes);
  }

  std::string_view get_algorithm_name() const override {
    return "BITPACK_STATIC";
  }

private:
  uint8_t bit_width_;

  // Helper functions for type conversion
  inline void convert_to_uint64(const void *input, uint64_t *output, size_t count, size_t value_size) const {
    const uint8_t *in = static_cast<const uint8_t *>(input);
    for (size_t i = 0; i < count; ++i) {
      output[i] = 0;
      std::memcpy(&output[i], in + i * value_size, std::min(value_size, sizeof(uint64_t)));
    }
  }

  inline void convert_from_uint64(const uint64_t *input, void *output, size_t count, size_t value_size) const {
    uint8_t *out = static_cast<uint8_t *>(output);
    for (size_t i = 0; i < count; ++i) {
      std::memcpy(out + i * value_size, &input[i], value_size);
    }
  }

  template <typename T>
  inline void write_value(std::vector<uint8_t> &result, size_t &pos, const T &value) const {
    std::memcpy(&result[pos], &value, sizeof(T));
    pos += sizeof(T);
  }

  template <typename T>
  inline T read_value(const std::vector<uint8_t> &data, size_t &pos) const {
    T value;
    std::memcpy(&value, &data[pos], sizeof(T));
    pos += sizeof(T);
    return value;
  }
};

// BitPack Dynamic compressor
// Analyzes data to find optimal bit width (95th percentile)
class BitPackDynamicCompressor : public BaseCompressor {
public:
  std::vector<uint8_t> compress(const void *data, size_t num_values, size_t value_size_bytes) override {
    std::vector<uint8_t> result;
    if (num_values == 0) {
      update_stats(0, 0, 0);
      return result;
    }

    size_t original_size = num_values * value_size_bytes;

    // Convert input to uint64_t array for analysis
    std::vector<uint64_t> values(num_values);
    convert_to_uint64(data, values.data(), num_values, value_size_bytes);

    // Find 95th percentile value for optimal bit width
    uint64_t p95_value = BitUtils::calculate_95th_percentile(values.data(), num_values);
    uint8_t optimal_bits = BitUtils::bits_needed(p95_value);

    // For values exceeding 95th percentile, we'll use overflow table
    std::vector<size_t> overflow_indices;
    std::vector<uint64_t> overflow_values;
    uint64_t max_packed_value = (1ULL << optimal_bits) - 1;

    for (size_t i = 0; i < num_values; ++i) {
      if (values[i] > max_packed_value) {
        overflow_indices.push_back(i);
        overflow_values.push_back(values[i]);
        values[i] = max_packed_value; // Mark as overflow
      }
    }

    // Calculate sizes
    size_t header_size = sizeof(size_t) + sizeof(size_t) + sizeof(uint8_t) + sizeof(size_t);
    size_t total_bits = num_values * optimal_bits;
    size_t packed_bytes = (total_bits + 7) / 8;
    size_t overflow_table_size = overflow_indices.size() * (sizeof(size_t) + sizeof(uint64_t));

    result.resize(header_size + packed_bytes + overflow_table_size);
    size_t pos = 0;

    // Write header: num_values, value_size_bytes, optimal_bits, overflow_count
    write_value(result, pos, num_values);
    write_value(result, pos, value_size_bytes);
    write_value(result, pos, optimal_bits);
    write_value(result, pos, overflow_indices.size());

    // Pack main data
    std::vector<uint8_t> packed_data;
    BitUtils::pack_bits(values.data(), num_values, optimal_bits, packed_data);
    std::memcpy(&result[pos], packed_data.data(), packed_data.size());
    pos += packed_data.size();

    // Write overflow table
    for (size_t i = 0; i < overflow_indices.size(); ++i) {
      write_value(result, pos, overflow_indices[i]);
      write_value(result, pos, overflow_values[i]);
    }

    update_stats(original_size, result.size(), num_values);
    return result;
  }

  void decompress(const std::vector<uint8_t> &compressed_data, void *output, size_t num_values, size_t value_size_bytes) override {
    if (compressed_data.size() < sizeof(size_t) + sizeof(size_t) + sizeof(uint8_t) + sizeof(size_t))
      return;

    size_t pos = 0;

    // Read header
    uint8_t optimal_bits = read_value<uint8_t>(compressed_data, pos);
    size_t overflow_count = read_value<size_t>(compressed_data, pos);

    assert(stored_num_values == num_values && stored_value_size == value_size_bytes);

    // Calculate packed data size
    size_t total_bits = num_values * optimal_bits;
    size_t packed_bytes = (total_bits + 7) / 8;

    // Extract packed data
    std::vector<uint8_t> packed_data(compressed_data.begin() + pos,
                                     compressed_data.begin() + pos + packed_bytes);
    pos += packed_bytes;

    // Unpack main data
    std::vector<uint64_t> values(num_values);
    BitUtils::unpack_bits(packed_data, values.data(), num_values, optimal_bits);

    // Apply overflow values
    for (size_t i = 0; i < overflow_count; ++i) {
      size_t index = read_value<size_t>(compressed_data, pos);
      uint64_t value = read_value<uint64_t>(compressed_data, pos);
      assert(index < num_values);
      values[index] = value;
    }

    // Convert back to original format
    convert_from_uint64(values.data(), output, num_values, value_size_bytes);
  }

  std::string_view get_algorithm_name() const override {
    return "BITPACK_DYNAMIC";
  }

private:
  // Helper functions (same as BitPackStaticCompressor)
  inline void convert_to_uint64(const void *input, uint64_t *output, size_t count, size_t value_size) const {
    const uint8_t *in = static_cast<const uint8_t *>(input);
    for (size_t i = 0; i < count; ++i) {
      output[i] = 0;
      std::memcpy(&output[i], in + i * value_size, std::min(value_size, sizeof(uint64_t)));
    }
  }

  inline void convert_from_uint64(const uint64_t *input, void *output, size_t count, size_t value_size) const {
    uint8_t *out = static_cast<uint8_t *>(output);
    for (size_t i = 0; i < count; ++i) {
      std::memcpy(out + i * value_size, &input[i], value_size);
    }
  }

  template <typename T>
  inline void write_value(std::vector<uint8_t> &result, size_t &pos, const T &value) const {
    std::memcpy(&result[pos], &value, sizeof(T));
    pos += sizeof(T);
  }

  template <typename T>
  inline T read_value(const std::vector<uint8_t> &data, size_t &pos) const {
    T value;
    std::memcpy(&value, &data[pos], sizeof(T));
    pos += sizeof(T);
    return value;
  }
};

} // namespace compress
} // namespace L2
