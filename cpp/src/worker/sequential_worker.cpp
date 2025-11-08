#include "worker/sequential_worker.hpp"
#include "worker/shared_state.hpp"

#include "codec/L2_DataType.hpp"
#include "codec/binary_decoder_L2.hpp"
#include "features/backend/FeatureStore.hpp"
#include "lob/LimitOrderBook.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

// ============================================================================
// ANALYSIS HELPER
// ============================================================================

static size_t process_binary_files(const AssetInfo::DateInfo &date_info,
                                   L2::BinaryDecoder_L2 &decoder,
                                   LimitOrderBook &lob) {
  size_t order_count = 0;
  if (!date_info.orders_file.empty()) {
    std::vector<L2::Order> decoded_orders;
    if (!decoder.decode_orders(date_info.orders_file, decoded_orders)) {
      return 0;
    }

    order_count = decoded_orders.size();
    for (const auto &ord : decoded_orders) {
      lob.process(ord);
    }
    lob.clear();
  }
  return order_count;
}

void sequential_worker(const SharedState &state,
                      int worker_id,
                      GlobalFeatureStore *feature_store,
                      misc::ProgressHandle progress_handle) {

  // Initialize as idle (will be updated if assets are assigned)
  progress_handle.set_label("Idle");
  progress_handle.update(1, 1, "");

  // Find assets assigned to this worker
  std::vector<size_t> my_asset_ids;
  size_t total_orders = 0;
  for (size_t i = 0; i < state.assets.size(); ++i) {
    if (state.assets[i].assigned_worker_id == worker_id) {
      my_asset_ids.push_back(i);
      total_orders += state.assets[i].get_total_order_count();
    }
  }

  // Initialize LOBs and decoders for each asset
  std::vector<std::unique_ptr<LimitOrderBook>> lobs;
  std::vector<std::unique_ptr<L2::BinaryDecoder_L2>> decoders;

  for (size_t asset_id : my_asset_ids) {
    const auto &asset = state.assets[asset_id];
    lobs.push_back(std::make_unique<LimitOrderBook>(
        L2::DEFAULT_ENCODER_ORDER_SIZE, asset.exchange_type, feature_store, asset.asset_id));
    decoders.push_back(std::make_unique<L2::BinaryDecoder_L2>(
        L2::DEFAULT_ENCODER_SNAPSHOT_SIZE, L2::DEFAULT_ENCODER_ORDER_SIZE));
  }

  // Progress label
  char label_buf[128];
  if (!my_asset_ids.empty()) {
    snprintf(label_buf, sizeof(label_buf), "%3zu Assets: %s(%s)",
             my_asset_ids.size(),
             state.assets[my_asset_ids[0]].asset_code.c_str(),
             state.assets[my_asset_ids[0]].asset_name.c_str());
  } else {
    snprintf(label_buf, sizeof(label_buf), "0 Assets");
  }
  progress_handle.set_label(label_buf);

  size_t cumulative_orders = 0;
  auto start_time = std::chrono::steady_clock::now();

  // Date-first traversal
  for (size_t date_idx = 0; date_idx < state.all_dates.size(); ++date_idx) {
    const std::string &date_str = state.all_dates[date_idx];

    // Process each asset at this date
    for (size_t i = 0; i < my_asset_ids.size(); ++i) {
      const size_t asset_id = my_asset_ids[i];
      const auto &asset = state.assets[asset_id];

      // Check if this asset has data for this date
      auto it = asset.date_info.find(date_str);
      if (it == asset.date_info.end()) {
        continue;
      }

      const auto &date_info = it->second;

      // Set date for feature computation
      lobs[i]->set_current_date(date_str);

      if (date_info.has_binaries()) {
        cumulative_orders += process_binary_files(date_info, *decoders[i], *lobs[i]);
      }
    }

    // Cross-sectional feature computation point
    // TODO: Insert cross-sectional factor calculation here

    // Update progress
    auto current_time = std::chrono::steady_clock::now();
    double elapsed_seconds = std::chrono::duration<double>(current_time - start_time).count();
    double speed_M_per_sec = (elapsed_seconds > 0) ? (cumulative_orders / 1e6) / elapsed_seconds : 0.0;

    char msg_buf[128];
    snprintf(msg_buf, sizeof(msg_buf), "%s [%.1fM/s (%.1fM)]", date_str.c_str(), speed_M_per_sec, total_orders / 1e6);
    progress_handle.update(date_idx + 1, state.all_dates.size(), msg_buf);
  }
}

