#pragma once

// Main compression header that includes all compression algorithms
#include "base_compressor.hpp"
#include "bitpack_compressor.hpp"
#include "column_compressor.hpp"
#include "custom_compressor.hpp"
#include "dictionary_compressor.hpp"
#include "rle_compressor.hpp"

namespace L2 {
namespace compress {

// Factory function to create compressors by algorithm type
inline std::unique_ptr<BaseCompressor> create_compressor(CompressionAlgo algo, uint8_t bit_width = 0) {
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

// Global column compressor instance
extern ColumnCompressor g_column_compressor;

} // namespace compress
} // namespace L2
