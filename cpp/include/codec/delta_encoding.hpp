#pragma once

#include <cstdint>
#include <cstdlib>

namespace L2 {
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

} // namespace DeltaUtils
} // namespace L2
