#pragma once

#include "lob/LimitOrderBookDefine.hpp"
#include "features/backend/FeatureStore.hpp"

// Tick-level sequential feature computation
// Input: LOB_Feature from LimitOrderBook
class Tick_Sequential {
public:
  explicit Tick_Sequential(const LOB_Feature* lob_feature)
      : lob_feature_(lob_feature) {}

  void set_store_context(GlobalFeatureStore* store, size_t asset_id) {
    feature_store_ = store;
    asset_id_ = asset_id;
  }

  // Main computation entry
  void compute_and_store() {
    // TODO: Implement tick-level feature computation
    // Extract features from lob_feature_ and store to feature_store_
  }

private:
  const LOB_Feature* lob_feature_;
  GlobalFeatureStore* feature_store_ = nullptr;
  size_t asset_id_ = 0;
};

