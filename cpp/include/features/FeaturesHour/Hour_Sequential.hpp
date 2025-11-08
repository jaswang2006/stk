#pragma once

#include "features/CoreSequential.hpp"
#include "features/backend/FeatureStore.hpp"

// Forward declaration
struct HourBar;

// Hour-level sequential feature computation
// Input: HourBar (resampled OHLCV data)
class Hour_Sequential {
public:
  explicit Hour_Sequential(const HourBar* hour_bar)
      : hour_bar_(hour_bar) {}

  void set_store_context(GlobalFeatureStore* store, size_t asset_id) {
    feature_store_ = store;
    asset_id_ = asset_id;
  }

  // Main computation entry
  void compute_and_store() {
    // TODO: Implement hour-level feature computation
    // Extract features from hour_bar_ and store to feature_store_
  }

private:
  const HourBar* hour_bar_;
  GlobalFeatureStore* feature_store_ = nullptr;
  size_t asset_id_ = 0;
};

