#pragma once

#include "../L2_DataType.hpp"
#include "base_compressor.hpp"
#include "bitpack_compressor.hpp"
#include "custom_compressor.hpp"
#include "dictionary_compressor.hpp"
#include "rle_compressor.hpp"
#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>

namespace L2 {
namespace compress {

// Column-wise compressor that manages compression for all schema fields
class ColumnCompressor {
public:
  ColumnCompressor() {
    initialize_compressors();
  }

  // Compress a complete Snapshot structure
  struct CompressedSnapshot {
    std::array<std::vector<uint8_t>, 18> column_data; // 18 columns in snapshot schema
    std::array<CompressionStats, 18> column_stats;
    size_t original_total_size = 0;
    size_t compressed_total_size = 0;
    double overall_compression_ratio = 0.0;
  };

  // Compress a complete Order structure
  struct CompressedOrder {
    std::array<std::vector<uint8_t>, 10> column_data; // 10 columns in order schema
    std::array<CompressionStats, 10> column_stats;
    size_t original_total_size = 0;
    size_t compressed_total_size = 0;
    double overall_compression_ratio = 0.0;
  };

  // Compress array of snapshots column by column
  CompressedSnapshot compress_snapshots(const std::vector<Snapshot> &snapshots) {
    CompressedSnapshot result;
    if (snapshots.empty())
      return result;

    (void)snapshots.size(); // Suppress unused variable warning

    // Extract and compress each column
    compress_snapshot_column(snapshots, result, 0, "hour", &Snapshot::hour, sizeof(uint8_t));
    compress_snapshot_column(snapshots, result, 1, "minute", &Snapshot::minute, sizeof(uint8_t));
    compress_snapshot_column(snapshots, result, 2, "second", &Snapshot::second, sizeof(uint8_t));
    compress_snapshot_column(snapshots, result, 3, "trade_count", &Snapshot::trade_count, sizeof(uint8_t));
    compress_snapshot_column(snapshots, result, 4, "volume", &Snapshot::volume, sizeof(uint16_t));
    compress_snapshot_column(snapshots, result, 5, "turnover", &Snapshot::turnover, sizeof(uint32_t));
    compress_snapshot_column(snapshots, result, 6, "high", &Snapshot::high, sizeof(uint16_t));
    compress_snapshot_column(snapshots, result, 7, "low", &Snapshot::low, sizeof(uint16_t));
    compress_snapshot_column(snapshots, result, 8, "close", &Snapshot::close, sizeof(uint16_t));
    compress_snapshot_column(snapshots, result, 9, "bid_price_ticks[10]", &Snapshot::bid_price_ticks, sizeof(uint16_t) * 10);
    compress_snapshot_column(snapshots, result, 10, "bid_volumes[10]", &Snapshot::bid_volumes, sizeof(uint16_t) * 10);
    compress_snapshot_column(snapshots, result, 11, "ask_price_ticks[10]", &Snapshot::ask_price_ticks, sizeof(uint16_t) * 10);
    compress_snapshot_column(snapshots, result, 12, "ask_volumes[10]", &Snapshot::ask_volumes, sizeof(uint16_t) * 10);
    compress_snapshot_column(snapshots, result, 13, "direction", &Snapshot::direction, sizeof(bool));
    compress_snapshot_column(snapshots, result, 14, "all_bid_vwap", &Snapshot::all_bid_vwap, sizeof(uint16_t));
    compress_snapshot_column(snapshots, result, 15, "all_ask_vwap", &Snapshot::all_ask_vwap, sizeof(uint16_t));
    compress_snapshot_column(snapshots, result, 16, "all_bid_volume", &Snapshot::all_bid_volume, sizeof(uint32_t));
    compress_snapshot_column(snapshots, result, 17, "all_ask_volume", &Snapshot::all_ask_volume, sizeof(uint32_t));

    // Calculate overall stats
    for (const auto &stats : result.column_stats) {
      result.original_total_size += stats.original_size_bytes;
      result.compressed_total_size += stats.compressed_size_bytes;
    }

    if (result.original_total_size > 0) {
      result.overall_compression_ratio = static_cast<double>(result.compressed_total_size) / result.original_total_size;
    }

    return result;
  }

  // Compress array of orders column by column
  CompressedOrder compress_orders(const std::vector<Order> &orders) {
    CompressedOrder result;
    if (orders.empty())
      return result;

    (void)orders.size(); // Suppress unused variable warning

    // Extract and compress each column
    compress_order_column(orders, result, 0, "hour", &Order::hour, sizeof(uint8_t));
    compress_order_column(orders, result, 1, "minute", &Order::minute, sizeof(uint8_t));
    compress_order_column(orders, result, 2, "second", &Order::second, sizeof(uint8_t));
    compress_order_column(orders, result, 3, "millisecond", &Order::millisecond, sizeof(uint8_t));
    compress_order_column(orders, result, 4, "order_type", &Order::order_type, sizeof(uint8_t));
    compress_order_column(orders, result, 5, "order_dir", &Order::order_dir, sizeof(uint8_t));
    compress_order_column(orders, result, 6, "price", &Order::price, sizeof(uint16_t));
    compress_order_column(orders, result, 7, "volume", &Order::volume, sizeof(uint16_t));
    compress_order_column(orders, result, 8, "bid_order_id", &Order::bid_order_id, sizeof(uint32_t));
    compress_order_column(orders, result, 9, "ask_order_id", &Order::ask_order_id, sizeof(uint32_t));

    // Calculate overall stats
    for (const auto &stats : result.column_stats) {
      result.original_total_size += stats.original_size_bytes;
      result.compressed_total_size += stats.compressed_size_bytes;
    }

    if (result.original_total_size > 0) {
      result.overall_compression_ratio = static_cast<double>(result.compressed_total_size) / result.original_total_size;
    }

    return result;
  }

  // Decompress snapshots
  std::vector<Snapshot> decompress_snapshots(const CompressedSnapshot &compressed_data, size_t count) {
    std::vector<Snapshot> result(count);
    if (count == 0)
      return result;

    // Decompress each column back into the struct array
    decompress_snapshot_column(compressed_data, result, 0, "hour", &Snapshot::hour, sizeof(uint8_t));
    decompress_snapshot_column(compressed_data, result, 1, "minute", &Snapshot::minute, sizeof(uint8_t));
    decompress_snapshot_column(compressed_data, result, 2, "second", &Snapshot::second, sizeof(uint8_t));
    decompress_snapshot_column(compressed_data, result, 3, "trade_count", &Snapshot::trade_count, sizeof(uint8_t));
    decompress_snapshot_column(compressed_data, result, 4, "volume", &Snapshot::volume, sizeof(uint16_t));
    decompress_snapshot_column(compressed_data, result, 5, "turnover", &Snapshot::turnover, sizeof(uint32_t));
    decompress_snapshot_column(compressed_data, result, 6, "high", &Snapshot::high, sizeof(uint16_t));
    decompress_snapshot_column(compressed_data, result, 7, "low", &Snapshot::low, sizeof(uint16_t));
    decompress_snapshot_column(compressed_data, result, 8, "close", &Snapshot::close, sizeof(uint16_t));
    decompress_snapshot_column(compressed_data, result, 9, "bid_price_ticks[10]", &Snapshot::bid_price_ticks, sizeof(uint16_t) * 10);
    decompress_snapshot_column(compressed_data, result, 10, "bid_volumes[10]", &Snapshot::bid_volumes, sizeof(uint16_t) * 10);
    decompress_snapshot_column(compressed_data, result, 11, "ask_price_ticks[10]", &Snapshot::ask_price_ticks, sizeof(uint16_t) * 10);
    decompress_snapshot_column(compressed_data, result, 12, "ask_volumes[10]", &Snapshot::ask_volumes, sizeof(uint16_t) * 10);
    decompress_snapshot_column(compressed_data, result, 13, "direction", &Snapshot::direction, sizeof(bool));
    decompress_snapshot_column(compressed_data, result, 14, "all_bid_vwap", &Snapshot::all_bid_vwap, sizeof(uint16_t));
    decompress_snapshot_column(compressed_data, result, 15, "all_ask_vwap", &Snapshot::all_ask_vwap, sizeof(uint16_t));
    decompress_snapshot_column(compressed_data, result, 16, "all_bid_volume", &Snapshot::all_bid_volume, sizeof(uint32_t));
    decompress_snapshot_column(compressed_data, result, 17, "all_ask_volume", &Snapshot::all_ask_volume, sizeof(uint32_t));

    return result;
  }

  // Decompress orders
  std::vector<Order> decompress_orders(const CompressedOrder &compressed_data, size_t count) {
    std::vector<Order> result(count);
    if (count == 0)
      return result;

    // Decompress each column back into the struct array
    decompress_order_column(compressed_data, result, 0, "hour", &Order::hour, sizeof(uint8_t));
    decompress_order_column(compressed_data, result, 1, "minute", &Order::minute, sizeof(uint8_t));
    decompress_order_column(compressed_data, result, 2, "second", &Order::second, sizeof(uint8_t));
    decompress_order_column(compressed_data, result, 3, "millisecond", &Order::millisecond, sizeof(uint8_t));
    decompress_order_column(compressed_data, result, 4, "order_type", &Order::order_type, sizeof(uint8_t));
    decompress_order_column(compressed_data, result, 5, "order_dir", &Order::order_dir, sizeof(uint8_t));
    decompress_order_column(compressed_data, result, 6, "price", &Order::price, sizeof(uint16_t));
    decompress_order_column(compressed_data, result, 7, "volume", &Order::volume, sizeof(uint16_t));
    decompress_order_column(compressed_data, result, 8, "bid_order_id", &Order::bid_order_id, sizeof(uint32_t));
    decompress_order_column(compressed_data, result, 9, "ask_order_id", &Order::ask_order_id, sizeof(uint32_t));

    return result;
  }

  // Print compression statistics
  void print_snapshot_stats(const CompressedSnapshot &compressed_data) const {
    std::cout << "\n=== Snapshot Compression Statistics ===" << std::endl;
    std::cout << "Column                | Algorithm        | Original (B) | Compressed (B) | Ratio  | Savings %" << std::endl;
    std::cout << "----------------------+------------------+--------------+----------------+--------+-----------" << std::endl;

    constexpr std::array<std::string_view, 18> column_names = {
        "hour", "minute", "second", "trade_count", "volume", "turnover",
        "high", "low", "close", "bid_price_ticks", "bid_volumes",
        "ask_price_ticks", "ask_volumes", "direction", "all_bid_vwap",
        "all_ask_vwap", "all_bid_volume", "all_ask_volume"};

    for (size_t i = 0; i < 18; ++i) {
      const auto &stats = compressed_data.column_stats[i];
      std::cout << std::setw(20) << std::left << column_names[i] << " | "
                << std::setw(16) << std::left << stats.algorithm_name << " | "
                << std::setw(12) << std::right << stats.original_size_bytes << " | "
                << std::setw(14) << std::right << stats.compressed_size_bytes << " | "
                << std::setw(6) << std::right << std::fixed << std::setprecision(3) << stats.compression_ratio << " | "
                << std::setw(9) << std::right << std::fixed << std::setprecision(1) << stats.space_saving_percent << "%" << std::endl;
    }

    std::cout << "----------------------+------------------+--------------+----------------+--------+-----------" << std::endl;
    std::cout << std::setw(20) << std::left << "TOTAL" << " | "
              << std::setw(16) << std::left << "COMBINED" << " | "
              << std::setw(12) << std::right << compressed_data.original_total_size << " | "
              << std::setw(14) << std::right << compressed_data.compressed_total_size << " | "
              << std::setw(6) << std::right << std::fixed << std::setprecision(3) << compressed_data.overall_compression_ratio << " | "
              << std::setw(9) << std::right << std::fixed << std::setprecision(1) << ((1.0 - compressed_data.overall_compression_ratio) * 100.0) << "%" << std::endl;
  }

  void print_order_stats(const CompressedOrder &compressed_data) const {
    std::cout << "\n=== Order Compression Statistics ===" << std::endl;
    std::cout << "Column                | Algorithm        | Original (B) | Compressed (B) | Ratio  | Savings %" << std::endl;
    std::cout << "----------------------+------------------+--------------+----------------+--------+-----------" << std::endl;

    constexpr std::array<std::string_view, 10> column_names = {
        "hour", "minute", "second", "millisecond", "order_type", "order_dir",
        "price", "volume", "bid_order_id", "ask_order_id"};

    for (size_t i = 0; i < 10; ++i) {
      const auto &stats = compressed_data.column_stats[i];
      std::cout << std::setw(20) << std::left << column_names[i] << " | "
                << std::setw(16) << std::left << stats.algorithm_name << " | "
                << std::setw(12) << std::right << stats.original_size_bytes << " | "
                << std::setw(14) << std::right << stats.compressed_size_bytes << " | "
                << std::setw(6) << std::right << std::fixed << std::setprecision(3) << stats.compression_ratio << " | "
                << std::setw(9) << std::right << std::fixed << std::setprecision(1) << stats.space_saving_percent << "%" << std::endl;
    }

    std::cout << "----------------------+------------------+--------------+----------------+--------+-----------" << std::endl;
    std::cout << std::setw(20) << std::left << "TOTAL" << " | "
              << std::setw(16) << std::left << "COMBINED" << " | "
              << std::setw(12) << std::right << compressed_data.original_total_size << " | "
              << std::setw(14) << std::right << compressed_data.compressed_total_size << " | "
              << std::setw(6) << std::right << std::fixed << std::setprecision(3) << compressed_data.overall_compression_ratio << " | "
              << std::setw(9) << std::right << std::fixed << std::setprecision(1) << ((1.0 - compressed_data.overall_compression_ratio) * 100.0) << "%" << std::endl;
  }

private:
  std::array<std::unique_ptr<BaseCompressor>, 18> snapshot_compressors_;
  std::array<std::unique_ptr<BaseCompressor>, 10> order_compressors_;

  void initialize_compressors() {
    // Initialize snapshot compressors based on schema
    constexpr size_t schema_size = sizeof(Snapshot_Schema) / sizeof(Snapshot_Schema[0]);
    static_assert(schema_size >= 18, "Schema size mismatch"); // At least 18 distinct columns

    for (size_t i = 0; i < 18; ++i) {
      snapshot_compressors_[i] = create_compressor_for_column(i, true);
    }

    for (size_t i = 0; i < 10; ++i) {
      order_compressors_[i] = create_compressor_for_column(i, false);
    }
  }

  std::unique_ptr<BaseCompressor> create_compressor_for_column(size_t column_idx, bool is_snapshot) {
    // Map column index to schema entry
    CompressionAlgo algo = CompressionAlgo::NONE; // Default to NONE
    uint8_t bit_width = 0;

    if (is_snapshot) {
      // Snapshot column mapping
      constexpr std::array<std::string_view, 18> snapshot_columns = {
          "hour", "minute", "second", "trade_count", "volume", "turnover",
          "high", "low", "close", "bid_price_ticks[10]", "bid_volumes[10]",
          "ask_price_ticks[10]", "ask_volumes[10]", "direction", "all_bid_vwap",
          "all_ask_vwap", "all_bid_volume", "all_ask_volume"};

      auto column_name = snapshot_columns[column_idx];
      constexpr size_t schema_size = sizeof(Snapshot_Schema) / sizeof(Snapshot_Schema[0]);

      for (size_t i = 0; i < schema_size; ++i) {
        if (Snapshot_Schema[i].column_name == column_name) {
          algo = Snapshot_Schema[i].algo;
          bit_width = Snapshot_Schema[i].bit_width;
          break;
        }
      }
    } else {
      // Order column mapping (all order columns are in the schema)
      constexpr std::array<std::string_view, 10> order_columns = {
          "hour", "minute", "second", "millisecond", "order_type", "order_dir",
          "price", "volume", "bid_order_id", "ask_order_id"};

      auto column_name = order_columns[column_idx];
      constexpr size_t schema_size = sizeof(Snapshot_Schema) / sizeof(Snapshot_Schema[0]);

      for (size_t i = 0; i < schema_size; ++i) {
        if (Snapshot_Schema[i].column_name == column_name) {
          algo = Snapshot_Schema[i].algo;
          bit_width = Snapshot_Schema[i].bit_width;
          break;
        }
      }
    }

    // Create appropriate compressor
    switch (algo) {
    case CompressionAlgo::RLE:
      return std::make_unique<RLECompressor>();
    case CompressionAlgo::DICTIONARY:
      return std::make_unique<DictionaryCompressor>();
    case CompressionAlgo::BITPACK_DYNAMIC:
      return std::make_unique<BitPackDynamicCompressor>();
    case CompressionAlgo::BITPACK_STATIC:
      return std::make_unique<BitPackStaticCompressor>(bit_width);
    case CompressionAlgo::CUSTOM:
      return std::make_unique<AutoSelectCompressor>();
    case CompressionAlgo::NONE:
    default:
      return std::make_unique<NoCompressor>();
    }
  }

  // Helper to compress a snapshot column with delta encoding support
  template <typename T>
  void compress_snapshot_column(const std::vector<Snapshot> &snapshots, CompressedSnapshot &result,
                                size_t column_idx, std::string_view column_name, T Snapshot::*member, size_t value_size) {
    if (snapshots.empty())
      return;

    // Extract column data
    std::vector<T> column_data;
    column_data.reserve(snapshots.size());
    for (const auto &snapshot : snapshots) {
      column_data.push_back(snapshot.*member);
    }

    // Apply delta encoding if specified in schema
    if (should_apply_delta_encoding(column_name)) {
      if constexpr (std::is_same_v<T, bool>) {
        // For bool vectors, we need to handle them differently since .data() is not available
        std::vector<uint8_t> bool_data(column_data.size());
        for (size_t i = 0; i < column_data.size(); ++i) {
          bool_data[i] = column_data[i] ? 1 : 0;
        }
        DeltaUtils::encode_deltas(bool_data.data(), bool_data.size());
        for (size_t i = 0; i < column_data.size(); ++i) {
          column_data[i] = bool_data[i] != 0;
        }
      } else {
        DeltaUtils::encode_deltas(column_data.data(), column_data.size());
      }
    }

    // Compress
    if constexpr (std::is_same_v<T, bool>) {
      // Convert bool vector to uint8_t for compression
      std::vector<uint8_t> bool_data(column_data.size());
      for (size_t i = 0; i < column_data.size(); ++i) {
        bool_data[i] = column_data[i] ? 1 : 0;
      }
      result.column_data[column_idx] = snapshot_compressors_[column_idx]->compress(
          bool_data.data(), bool_data.size(), sizeof(uint8_t));
    } else {
      result.column_data[column_idx] = snapshot_compressors_[column_idx]->compress(
          column_data.data(), column_data.size(), value_size);
    }
    result.column_stats[column_idx] = snapshot_compressors_[column_idx]->get_stats();
  }

  // Specialization for array types (like uint16_t[10])
  template <typename T, size_t N>
  void compress_snapshot_column(const std::vector<Snapshot> &snapshots, CompressedSnapshot &result,
                                size_t column_idx, std::string_view column_name, T (Snapshot::*member)[N], size_t /* value_size */) {
    if (snapshots.empty())
      return;

    // Extract column data as flattened array
    std::vector<T> column_data;
    column_data.reserve(snapshots.size() * N);
    for (const auto &snapshot : snapshots) {
      const T *array_data = snapshot.*member;
      for (size_t i = 0; i < N; ++i) {
        column_data.push_back(array_data[i]);
      }
    }

    // Apply delta encoding if specified in schema
    if (should_apply_delta_encoding(column_name)) {
      DeltaUtils::encode_deltas(column_data.data(), column_data.size());
    }

    // Compress
    result.column_data[column_idx] = snapshot_compressors_[column_idx]->compress(
        column_data.data(), column_data.size(), sizeof(T));
    result.column_stats[column_idx] = snapshot_compressors_[column_idx]->get_stats();
  }

  // Helper to compress an order column with delta encoding support
  template <typename T>
  void compress_order_column(const std::vector<Order> &orders, CompressedOrder &result,
                             size_t column_idx, std::string_view column_name, T Order::*member, size_t value_size) {
    if (orders.empty())
      return;

    // Extract column data
    std::vector<T> column_data;
    column_data.reserve(orders.size());
    for (const auto &order : orders) {
      column_data.push_back(order.*member);
    }

    // Apply delta encoding if specified in schema
    if (should_apply_delta_encoding(column_name)) {
      DeltaUtils::encode_deltas(column_data.data(), column_data.size());
    }

    // Compress
    result.column_data[column_idx] = order_compressors_[column_idx]->compress(
        column_data.data(), column_data.size(), value_size);
    result.column_stats[column_idx] = order_compressors_[column_idx]->get_stats();
  }

  // Helper to decompress a snapshot column with delta decoding support
  template <typename T>
  void decompress_snapshot_column(const CompressedSnapshot &compressed_data, std::vector<Snapshot> &result,
                                  size_t column_idx, std::string_view column_name, T Snapshot::*member, size_t value_size) {
    if (result.empty())
      return;

    // Decompress column data
    std::vector<T> column_data(result.size());
    if constexpr (std::is_same_v<T, bool>) {
      // For bool vectors, decompress as uint8_t first
      std::vector<uint8_t> bool_data(result.size());
      snapshot_compressors_[column_idx]->decompress(
          compressed_data.column_data[column_idx], bool_data.data(), result.size(), sizeof(uint8_t));

      // Convert back to bool
      for (size_t i = 0; i < result.size(); ++i) {
        column_data[i] = bool_data[i] != 0;
      }
    } else {
      snapshot_compressors_[column_idx]->decompress(
          compressed_data.column_data[column_idx], column_data.data(), result.size(), value_size);
    }

    // Apply delta decoding if specified in schema
    if (should_apply_delta_encoding(column_name)) {
      if constexpr (std::is_same_v<T, bool>) {
        // For bool vectors, we need to handle them differently since .data() is not available
        std::vector<uint8_t> bool_data(column_data.size());
        for (size_t i = 0; i < column_data.size(); ++i) {
          bool_data[i] = column_data[i] ? 1 : 0;
        }
        DeltaUtils::decode_deltas(bool_data.data(), bool_data.size());
        for (size_t i = 0; i < column_data.size(); ++i) {
          column_data[i] = bool_data[i] != 0;
        }
      } else {
        DeltaUtils::decode_deltas(column_data.data(), column_data.size());
      }
    }

    // Copy back to result
    for (size_t i = 0; i < result.size(); ++i) {
      result[i].*member = column_data[i];
    }
  }

  // Specialization for array types (like uint16_t[10])
  template <typename T, size_t N>
  void decompress_snapshot_column(const CompressedSnapshot &compressed_data, std::vector<Snapshot> &result,
                                  size_t column_idx, std::string_view column_name, T (Snapshot::*member)[N], size_t /* value_size */) {
    if (result.empty())
      return;

    // Decompress column data as flattened array
    std::vector<T> column_data(result.size() * N);
    snapshot_compressors_[column_idx]->decompress(
        compressed_data.column_data[column_idx], column_data.data(), result.size() * N, sizeof(T));

    // Apply delta decoding if specified in schema
    if (should_apply_delta_encoding(column_name)) {
      DeltaUtils::decode_deltas(column_data.data(), column_data.size());
    }

    // Copy back to result
    for (size_t i = 0; i < result.size(); ++i) {
      T *array_data = result[i].*member;
      for (size_t j = 0; j < N; ++j) {
        array_data[j] = column_data[i * N + j];
      }
    }
  }

  // Helper to decompress an order column with delta decoding support
  template <typename T>
  void decompress_order_column(const CompressedOrder &compressed_data, std::vector<Order> &result,
                               size_t column_idx, std::string_view column_name, T Order::*member, size_t value_size) {
    if (result.empty())
      return;

    // Decompress column data
    std::vector<T> column_data(result.size());
    order_compressors_[column_idx]->decompress(
        compressed_data.column_data[column_idx], column_data.data(), result.size(), value_size);

    // Apply delta decoding if specified in schema
    if (should_apply_delta_encoding(column_name)) {
      DeltaUtils::decode_deltas(column_data.data(), column_data.size());
    }

    // Copy back to result
    for (size_t i = 0; i < result.size(); ++i) {
      result[i].*member = column_data[i];
    }
  }

  // Check if delta encoding should be applied based on schema
  bool should_apply_delta_encoding(std::string_view column_name) const {
    constexpr size_t schema_size = sizeof(Snapshot_Schema) / sizeof(Snapshot_Schema[0]);

    for (size_t i = 0; i < schema_size; ++i) {
      if (Snapshot_Schema[i].column_name == column_name) {
        return Snapshot_Schema[i].use_delta;
      }
    }
    return false;
  }
};

} // namespace compress
} // namespace L2
