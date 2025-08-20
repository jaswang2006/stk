#pragma once
#include <cmath>

// #define KDE_ESTIMATE

#ifdef KDE_ESTIMATE
#include <array>
#endif

// include <iostream>
// include "RollingZScore.hpp"
//
// int  main() {
//    RollingZScore<float, 3> rz;
//    float data[] = {1, 2, 2, 3, 4, 6, 5, 4, 3, 2};
//
//    for (float x : data) {
//        float z = rz.update(x);
//        std::cout << z << "\n";
//    }
//

template <typename T, size_t N>
class RollingZScore {
public:
  explicit RollingZScore()
      : buf{}, idx(0), count(0), M2(0), mean(0), stddev(0), zs(0) {}

  // Update with new value, return z-score
  inline T update(T x) noexcept {
#ifdef KDE_ESTIMATE
    // Initialize histogram on first call
    if (!kde_initialized) {
      kde_initialized = true;
      kde_min = kde_max = x;
    }
    
    // Update histogram range
    if (x < kde_min) kde_min = x;
    if (x > kde_max) kde_max = x;
    
    // Map to bin and increment
    const T range = kde_max - kde_min;
    const int bin_idx = range > 0 ? (x - kde_min) / range * (NUM_BINS - 1) : NUM_BINS / 2;
    bins[bin_idx]++;
    kde_count++;
#endif

    T old = buf[idx];
    buf[idx] = x;
    idx = (idx + 1) % N;

    if (count < N) [[unlikely]] {
      count++;
      T delta = x - mean;
      mean += delta / count;
      M2 += delta * (x - mean);
    } else {
      T old_mean = mean;
      mean += (x - old) / N;
      M2 += (x - old) * (x - mean + old - old_mean);
    }

#ifdef KDE_ESTIMATE
    // Use histogram for z-score computation
    T sum_x = 0, sum_x2 = 0;
    const T bin_width = range / NUM_BINS;
    for (int i = 0; i < NUM_BINS; ++i) {
      const T bin_center = kde_min + (i + 0.5) * bin_width;
      sum_x += bins[i] * bin_center;
      sum_x2 += bins[i] * bin_center * bin_center;
    }
    mean = sum_x / kde_count;
    const T variance = sum_x2 / kde_count - mean * mean;
    stddev = std::sqrt(variance);
    zs = (x - mean) / stddev;
#else
    T variance = (count > 1) ? M2 / (count - 1) : T(0.0);
    stddev = std::sqrt(variance);
    zs = (stddev > 1e-12) ? (x - mean) / stddev : T(0);
#endif
    return zs;
  }

  // Constant-time accessors
  inline T get_mean() const noexcept { return mean; }
  inline T get_stddev() const noexcept { return stddev; }
  inline T get_zscore() const noexcept { return zs; }

public:
#ifdef KDE_ESTIMATE
  static constexpr size_t NUM_BINS = 100;
#endif

private:
  T buf[N]; // circular buffer
  size_t idx;
  size_t count;
  T M2;
  T mean;
  T stddev;
  T zs;
  
#ifdef KDE_ESTIMATE
  // Histogram for entire history
  bool kde_initialized = false;
  size_t kde_count = 0;
  T kde_min = static_cast<T>(0);
  T kde_max = static_cast<T>(0);
  std::array<uint32_t, NUM_BINS> bins{};
#endif
};