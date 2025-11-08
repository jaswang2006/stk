#include "worker/crosssectional_worker.hpp"
#include "worker/shared_state.hpp"

#include "features/backend/FeatureStore.hpp"
#include "features/CoreCrosssection.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>

void crosssectional_worker(const SharedState& state,
                          GlobalFeatureStore* feature_store,
                          misc::ProgressHandle progress_handle) {
  
  progress_handle.set_label("CS Worker");
  
  size_t total_time_slots = 0;
  size_t processed_slots = 0;
  
  // Date-first traversal
  for (size_t date_idx = 0; date_idx < state.all_dates.size(); ++date_idx) {
    const std::string& date_str = state.all_dates[date_idx];
    constexpr size_t level_idx = 0;
    const size_t capacity = feature_store->get_T(level_idx);
    
    // Process each time slot as it becomes ready
    size_t t = 0;
    while (t < capacity) {
      // Wait for time slot t to be ready (simple polling)
      while (!feature_store->is_timeslot_ready(date_str, level_idx, t)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      
      // Compute CS features for this time slot
      compute_cs_for_timeslot(feature_store, date_str, t);
      
      ++t;
      ++processed_slots;
      
      // Update progress every 100 time slots
      if (t % 100 == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s t=%zu/%zu",
                date_str.c_str(), t, capacity);
        progress_handle.update(processed_slots, total_time_slots, msg);
      }
    }
    
    total_time_slots += t;
    
    // Mark this date as complete for tensor pool recycling
    feature_store->mark_date_complete(date_str);
  }
  
  progress_handle.update(processed_slots, total_time_slots, "Complete");
}

