#pragma once

#include "features/CoreSequential.hpp"
#include "features/backend/FeatureStore.hpp"

// Forward declaration
struct MinuteBar;

// Minute-level sequential feature computation
// Input: MinuteBar (resampled OHLCV data)
class Minute_Sequential {
public:
  explicit Minute_Sequential(const MinuteBar* minute_bar)
      : minute_bar_(minute_bar) {}

  void set_store_context(GlobalFeatureStore* store, size_t asset_id) {
    feature_store_ = store;
    asset_id_ = asset_id;
  }

  // Main computation entry
  void compute_and_store() {
    // TODO: Implement minute-level feature computation
    // Extract features from minute_bar_ and store to feature_store_
  }

private:
  const MinuteBar* minute_bar_;
  GlobalFeatureStore* feature_store_ = nullptr;
  size_t asset_id_ = 0;
};

