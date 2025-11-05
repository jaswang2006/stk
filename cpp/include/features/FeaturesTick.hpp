#pragma once

#include "backend/FeatureStoreConfig.hpp"
#include "backend/FeatureStore.hpp"
#include "lob/LimitOrderBookDefine.hpp"
#include "math/normalize/RollingZScore.hpp"
#include <cmath>

// ============================================================================
// LEVEL 0 FEATURES - Tick-level Computation
// ============================================================================
// Clean interface like old architecture: compute -> fill struct -> push

#define TICK_SIZE 0.01f
static constexpr int ZSCORE_WINDOW = 1800;

class FeaturesTick {
private:
  const LOB_Feature* lob_feature_;
  GlobalFeatureStore* global_store_;
  size_t asset_id_;
  
  RollingZScore<float, ZSCORE_WINDOW> zs_spread_;
  RollingZScore<float, ZSCORE_WINDOW> zs_tobi_;
  RollingZScore<float, ZSCORE_WINDOW> zs_mpg_;

public:
  FeaturesTick(const LOB_Feature* lob_feature)
      : lob_feature_(lob_feature),
        global_store_(nullptr),
        asset_id_(0) {}

  // Set store context (called once at LOB construction)
  void set_store_context(GlobalFeatureStore* store, size_t asset_id) {
    global_store_ = store;
    asset_id_ = asset_id;
  }

  // Compute and store - clean like old architecture
  void compute_and_store() {
    if (!lob_feature_->depth_updated || !global_store_) {
      return;
    }

    const auto &depth_buffer = lob_feature_->depth_buffer;
    if (depth_buffer.size() < 2 * LOB_FEATURE_DEPTH_LEVELS) [[unlikely]] {
      return;
    }

    // Step 1: Create data structure
    Level0Data data = {};

    // Step 2: Extract LOB data
    const Level *best_ask_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS - 1];
    const Level *best_bid_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS];

    const float best_bid_price = static_cast<float>(best_bid_level->price) * 0.01f;
    const float best_ask_price = static_cast<float>(best_ask_level->price) * 0.01f;
    const float best_bid_volume = static_cast<float>(std::abs(best_bid_level->net_quantity));
    const float best_ask_volume = static_cast<float>(std::abs(best_ask_level->net_quantity));

    // Step 3: Compute all features and fill struct
    data.timestamp = 0;  // TODO: from LOB
    
    data.mid_price = (best_bid_price + best_ask_price) * 0.5f;
    data.spread = best_ask_price - best_bid_price;
    data.spread_z = zs_spread_.update(data.spread);
    
    const float tobi_denom = best_bid_volume + best_ask_volume;
    data.tobi = (tobi_denom > 0.0f) ? (best_bid_volume - best_ask_volume) / tobi_denom : 0.0f;
    data.tobi_z = zs_tobi_.update(data.tobi);
    
    const float denom = best_bid_volume + best_ask_volume;
    data.micro_price = (denom > 0.0f)
                          ? ((best_ask_price * best_bid_volume + best_bid_price * best_ask_volume) / denom)
                          : data.mid_price;
    data.mpg = data.micro_price - data.mid_price;
    data.mpg_z = zs_mpg_.update(data.mpg);

    // Step 4: Ultra-fast single-line push (billions of calls optimized!)
    global_store_->push(L0_INDEX, asset_id_, &data);
  }
};
