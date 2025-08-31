#pragma once

#include <cstdint>

namespace L2 {
namespace Parallel {

// Simplified configuration for high-performance processing
struct ProcessingConfig {
  const uint32_t decompression_threads = 8;
  const uint32_t max_temp_folders = 16;  // disk backpressure limit

  const char *input_base = "/media/chuyin/48ac8067-d3b7-4332-b652-45e367a1ebcc/A_stock/L2";
  const char *output_base = "/home/chuyin/work/L2_binary"; // "/mnt/dev/sde/A_stock/L2_binary";
  const char *temp_base = "../../../output/tmp";

  // Debug option to skip decompression and encode directly from input_base
  bool skip_decompression = false;
};

// Global configuration instance
extern ProcessingConfig g_config;

} // namespace Parallel
} // namespace L2
