#pragma once

#include "base_compressor.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace L2 {
namespace compress {

// Run Length Encoding compressor
// Optimal for data with long runs of identical values (volume, turnover with many zeros)
class RLECompressor : public BaseCompressor {
public:
  std::vector<uint8_t> compress(const void *data, size_t num_values, size_t value_size_bytes) override {
    std::vector<uint8_t> result;
    if (num_values == 0) {
      update_stats(0, 0, 0);
      return result;
    }

    const uint8_t *input = static_cast<const uint8_t *>(data);
    size_t original_size = num_values * value_size_bytes;

    // Reserve space for header (num_values + value_size) + worst case (2x original)
    result.reserve(sizeof(size_t) + sizeof(size_t) + original_size * 2);

    // Write header: num_values and value_size_bytes
    write_value(result, num_values);
    write_value(result, value_size_bytes);

    size_t i = 0;
    while (i < num_values) {
      // Find run length of current value
      size_t run_length = 1;
      while (run_length < 255 && i + run_length < num_values &&
             values_equal(input + i * value_size_bytes,
                          input + (i + run_length) * value_size_bytes,
                          value_size_bytes)) {
        run_length++;
      }

      // Write run: [length][value]
      result.push_back(static_cast<uint8_t>(run_length));
      result.insert(result.end(),
                    input + i * value_size_bytes,
                    input + (i + 1) * value_size_bytes);

      i += run_length;
    }

    update_stats(original_size, result.size(), num_values);
    return result;
  }

  void decompress(const std::vector<uint8_t> &compressed_data, void *output, size_t num_values, size_t value_size_bytes) override {
    if (compressed_data.size() < 2 * sizeof(size_t))
      return;

    uint8_t *out = static_cast<uint8_t *>(output);
    size_t pos = 0;

    // Read header
    size_t output_pos = 0;

    while (pos < compressed_data.size() && output_pos < num_values) {
      // Read run length
      uint8_t run_length = compressed_data[pos++];

      // Read value
      if (pos + value_size_bytes > compressed_data.size())
        break;

      // Copy value run_length times
      for (uint8_t i = 0; i < run_length && output_pos < num_values; ++i) {
        std::memcpy(out + output_pos * value_size_bytes,
                    &compressed_data[pos],
                    value_size_bytes);
        output_pos++;
      }

      pos += value_size_bytes;
    }
  }

  std::string_view get_algorithm_name() const override {
    return "RLE";
  }

private:
  // Helper to compare two values of arbitrary size
  inline bool values_equal(const uint8_t *a, const uint8_t *b, size_t size) const {
    return std::memcmp(a, b, size) == 0;
  }

  // Helper to write a value to the result vector
  template <typename T>
  inline void write_value(std::vector<uint8_t> &result, const T &value) const {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);
    result.insert(result.end(), bytes, bytes + sizeof(T));
  }

  // Helper to read a value from compressed data
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
