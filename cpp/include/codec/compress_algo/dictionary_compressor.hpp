#pragma once

#include "base_compressor.hpp"
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace L2 {
namespace compress {

// Dictionary compressor
// Optimal for data with small number of distinct values (direction, order_type, order_dir)
class DictionaryCompressor : public BaseCompressor {
public:
  std::vector<uint8_t> compress(const void *data, size_t num_values, size_t value_size_bytes) override {
    std::vector<uint8_t> result;
    if (num_values == 0) {
      update_stats(0, 0, 0);
      return result;
    }

    const uint8_t *input = static_cast<const uint8_t *>(data);
    size_t original_size = num_values * value_size_bytes;

    // Build dictionary of unique values
    std::vector<std::vector<uint8_t>> unique_values;
    std::unordered_map<std::string, uint8_t> value_to_index;

    // Scan for unique values
    for (size_t i = 0; i < num_values; ++i) {
      std::string value_key(input + i * value_size_bytes,
                            input + (i + 1) * value_size_bytes);

      if (value_to_index.find(value_key) == value_to_index.end()) {
        if (unique_values.size() >= 255) {
          // Too many unique values for dictionary compression
          // Fall back to no compression
          result.resize(sizeof(size_t) + sizeof(size_t) + sizeof(uint8_t) + original_size);
          size_t pos = 0;
          write_value(result, pos, num_values);
          write_value(result, pos, value_size_bytes);
          write_value(result, pos, static_cast<uint8_t>(0)); // 0 unique values means no compression
          std::memcpy(&result[pos], input, original_size);
          update_stats(original_size, result.size(), num_values);
          return result;
        }

        value_to_index[value_key] = static_cast<uint8_t>(unique_values.size());
        unique_values.emplace_back(input + i * value_size_bytes,
                                   input + (i + 1) * value_size_bytes);
      }
    }

    // Calculate compressed size
    size_t dictionary_size = unique_values.size() * value_size_bytes;
    size_t indices_size = num_values; // 1 byte per index
    size_t header_size = sizeof(size_t) + sizeof(size_t) + sizeof(uint8_t);

    result.resize(header_size + dictionary_size + indices_size);
    size_t pos = 0;

    // Write header: num_values, value_size_bytes, num_unique_values
    write_value(result, pos, num_values);
    write_value(result, pos, value_size_bytes);
    write_value(result, pos, static_cast<uint8_t>(unique_values.size()));

    // Write dictionary
    for (const auto &unique_value : unique_values) {
      std::memcpy(&result[pos], unique_value.data(), value_size_bytes);
      pos += value_size_bytes;
    }

    // Write indices
    for (size_t i = 0; i < num_values; ++i) {
      std::string value_key(input + i * value_size_bytes,
                            input + (i + 1) * value_size_bytes);
      result[pos++] = value_to_index[value_key];
    }

    update_stats(original_size, result.size(), num_values);
    return result;
  }

  void decompress(const std::vector<uint8_t> &compressed_data, void *output, size_t num_values, size_t value_size_bytes) override {
    if (compressed_data.size() < sizeof(size_t) + sizeof(size_t) + sizeof(uint8_t))
      return;

    uint8_t *out = static_cast<uint8_t *>(output);
    size_t pos = 0;

    // Read header
    uint8_t num_unique_values = read_value<uint8_t>(compressed_data, pos);

    if (num_unique_values == 0) {
      // No compression was applied, copy directly
      std::memcpy(output, &compressed_data[pos], num_values * value_size_bytes);
      return;
    }

    // Read dictionary
    std::vector<std::vector<uint8_t>> dictionary(num_unique_values);
    for (uint8_t i = 0; i < num_unique_values; ++i) {
      dictionary[i].resize(value_size_bytes);
      std::memcpy(dictionary[i].data(), &compressed_data[pos], value_size_bytes);
      pos += value_size_bytes;
    }

    // Read indices and reconstruct values
    for (size_t i = 0; i < num_values; ++i) {
      uint8_t index = compressed_data[pos++];
      assert(index < num_unique_values);
      std::memcpy(out + i * value_size_bytes,
                  dictionary[index].data(),
                  value_size_bytes);
    }
  }

  std::string_view get_algorithm_name() const override {
    return "DICTIONARY";
  }

private:
  // Helper to write a value to the result vector at position
  template <typename T>
  inline void write_value(std::vector<uint8_t> &result, size_t &pos, const T &value) const {
    std::memcpy(&result[pos], &value, sizeof(T));
    pos += sizeof(T);
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
