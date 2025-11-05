#pragma once

#include "backend/FeatureStoreConfig.hpp"
#include "backend/FeatureStore.hpp"
#include <cmath>
#include <string>

// ============================================================================
// LEVEL 1 FEATURES - Minute-level Aggregation
// ============================================================================

class FeaturesMinute {
private:
  GlobalFeatureStore* global_store_;
  size_t asset_id_;
  std::string current_date_;
  
  // Accumulation state
  uint32_t tick_count_ = 0;
  double sum_pv_ = 0.0;
  double sum_v_ = 0.0;
  float high_ = -1e9f;
  float low_ = 1e9f;
  float open_ = 0.0f;
  float close_ = 0.0f;
  float sum_spread_ = 0.0f;
  float sum_tobi_ = 0.0f;
  double sum_price_sq_ = 0.0;
  double sum_price_ = 0.0;

public:
  FeaturesMinute(GlobalFeatureStore* store,
                 size_t asset_id,
                 const std::string& date)
      : global_store_(store),
        asset_id_(asset_id),
        current_date_(date) {}

  void reset() {
    tick_count_ = 0;
    sum_pv_ = 0.0;
    sum_v_ = 0.0;
    high_ = -1e9f;
    low_ = 1e9f;
    open_ = 0.0f;
    close_ = 0.0f;
    sum_spread_ = 0.0f;
    sum_tobi_ = 0.0f;
    sum_price_sq_ = 0.0;
    sum_price_ = 0.0;
  }

  void accumulate_tick(float mid_price, float spread, float tobi, float volume = 1.0f) {
    tick_count_++;
    sum_pv_ += mid_price * volume;
    sum_v_ += volume;
    
    if (tick_count_ == 1) open_ = mid_price;
    close_ = mid_price;
    if (mid_price > high_) high_ = mid_price;
    if (mid_price < low_) low_ = mid_price;
    
    sum_spread_ += spread;
    sum_tobi_ += tobi;
    sum_price_ += mid_price;
    sum_price_sq_ += mid_price * mid_price;
  }

  // Compute and store - clean interface
  void compute_and_store() {
    if (tick_count_ == 0) {
      return;
    }

    // Step 1: Create data structure
    Level1Data data = {};

    // Step 2: Compute aggregated features
    data.timestamp = 0;  // TODO
    data.vwap = (sum_v_ > 0.0) ? static_cast<float>(sum_pv_ / sum_v_) : 0.0f;
    data.high = high_;
    data.low = low_;
    data.open = open_;
    data.close = close_;
    data.tick_count = static_cast<float>(tick_count_);
    data.mean_spread = sum_spread_ / tick_count_;
    data.mean_tobi = sum_tobi_ / tick_count_;
    
    const double mean_price = sum_price_ / tick_count_;
    const double variance = (sum_price_sq_ / tick_count_) - (mean_price * mean_price);
    data.volatility = static_cast<float>(variance > 0 ? std::sqrt(variance) : 0.0);

    // Step 3: Ultra-fast single-line push (auto-links to parent L0)
    global_store_->push(L1_INDEX, asset_id_, &data);
  }
};
