#pragma once

#include "FeatureStoreConfig.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// FEATURE STORE IMPLEMENTATION - HIGH-PERFORMANCE TIME-SERIES STORAGE
// ============================================================================
// Design principles:
// - Time-major layout: [time × asset × feature] for sequential access
// - Pre-allocated arrays: zero dynamic allocation in hot path
// - Aligned memory: 64-byte alignment for SIMD/cache optimization
// - Parent linking: hierarchical pointer chain for multi-level aggregation
// - Macro-generated: fully automatic code generation for all levels
//
// Memory efficiency:
// - Uniform float storage: 4 bytes per feature
// - Contiguous layout: cache-friendly sequential writes
// - Pre-allocated capacity: eliminates reallocation overhead
// ============================================================================

// ============================================================================
// LEVEL STORAGE - SINGLE-LEVEL MULTI-ASSET BUFFER
// ============================================================================
// Memory layout: [time₀...timeₜ] × [asset₀...assetₙ] × [feature₀...featureₖ]
// Parent indices: [time₀...timeₜ] × [asset₀...assetₙ] → parent_time_idx

class LevelStorage {
private:
  // Core storage
  float *data_ = nullptr;            // Feature data: time × asset × feature
  size_t *parent_indices_ = nullptr; // Parent time indices: time × asset

  // Dimension metadata
  size_t num_features_;       // Number of features per row
  size_t num_assets_;         // Number of assets
  size_t capacity_per_asset_; // Pre-allocated time steps per asset
  
  // Per-asset time tracking (each asset independently increments)
  std::vector<size_t> per_asset_time_idx_;
  
  // Dynamic expansion configuration
  static constexpr size_t EXPANSION_CHUNK = 100'000; // Expand by 100K rows each time

public:
  // ============================================================================
  // LIFECYCLE MANAGEMENT
  // ============================================================================

  LevelStorage() = default;

  ~LevelStorage() {
    if (data_)
      std::free(data_);
    if (parent_indices_)
      std::free(parent_indices_);
  }

  LevelStorage(const LevelStorage &) = delete;
  LevelStorage &operator=(const LevelStorage &) = delete;

  // Initialize with pre-allocation (called once per date-level pair)
  void initialize(size_t num_features, size_t num_assets, size_t capacity_per_asset) {
    // Prevent double initialization
    if (data_) {
      std::free(data_);
      data_ = nullptr;
    }
    if (parent_indices_) {
      std::free(parent_indices_);
      parent_indices_ = nullptr;
    }
    
    num_features_ = num_features;
    num_assets_ = num_assets;
    capacity_per_asset_ = capacity_per_asset;
    
    // Initialize per-asset time indices
    per_asset_time_idx_.assign(num_assets, 0);

    // Allocate feature data: time × asset × feature
    const size_t total_floats = capacity_per_asset * num_assets * num_features;
    data_ = static_cast<float *>(std::aligned_alloc(64, total_floats * sizeof(float)));
    assert(data_ && "aligned_alloc failed for data_");
    std::memset(data_, 0, total_floats * sizeof(float));

    // Allocate parent linkage: time × asset
    const size_t total_parent_slots = capacity_per_asset * num_assets;
    parent_indices_ = static_cast<size_t *>(std::aligned_alloc(64, total_parent_slots * sizeof(size_t)));
    assert(parent_indices_ && "aligned_alloc failed for parent_indices_");
    std::memset(parent_indices_, 0, total_parent_slots * sizeof(size_t));
  }
  
  // ============================================================================
  // CAPACITY MANAGEMENT
  // ============================================================================
  
  // Check if specific asset needs expansion
  bool needs_expansion(size_t asset_id) const {
    return per_asset_time_idx_[asset_id] >= capacity_per_asset_;
  }
  
  // Expand capacity by EXPANSION_CHUNK rows
  void expand_capacity() {
    const size_t old_capacity = capacity_per_asset_;
    const size_t new_capacity = old_capacity + EXPANSION_CHUNK;
    
    // Reallocate data array
    const size_t old_total_floats = old_capacity * num_assets_ * num_features_;
    const size_t new_total_floats = new_capacity * num_assets_ * num_features_;
    
    float *new_data = static_cast<float *>(std::aligned_alloc(64, new_total_floats * sizeof(float)));
    assert(new_data && "aligned_alloc failed during expansion");
    
    std::memcpy(new_data, data_, old_total_floats * sizeof(float));
    std::memset(new_data + old_total_floats, 0, (new_total_floats - old_total_floats) * sizeof(float));
    std::free(data_);
    data_ = new_data;
    
    // Reallocate parent indices
    const size_t old_total_slots = old_capacity * num_assets_;
    const size_t new_total_slots = new_capacity * num_assets_;
    
    size_t *new_parent_indices = static_cast<size_t *>(std::aligned_alloc(64, new_total_slots * sizeof(size_t)));
    assert(new_parent_indices && "aligned_alloc failed during expansion");
    
    std::memcpy(new_parent_indices, parent_indices_, old_total_slots * sizeof(size_t));
    std::memset(new_parent_indices + old_total_slots, 0, (new_total_slots - old_total_slots) * sizeof(size_t));
    std::free(parent_indices_);
    parent_indices_ = new_parent_indices;
    
    capacity_per_asset_ = new_capacity;
  }

  // ============================================================================
  // WRITE OPERATIONS
  // ============================================================================

  // Push complete row for specific asset (MACRO-GENERATED specializations below)
  // Auto-increments per-asset time index after write
  template <typename LevelData>
  void push_row(size_t asset_id, const LevelData &data, size_t parent_row_idx);

  // ============================================================================
  // PARENT LINKAGE ACCESS
  // ============================================================================

  void set_parent_index(size_t asset_id, size_t time_idx, size_t parent_row_idx) {
    const size_t flat_idx = time_idx * num_assets_ + asset_id;
    parent_indices_[flat_idx] = parent_row_idx;
  }

  size_t get_parent_index(size_t asset_id, size_t time_idx) const {
    const size_t flat_idx = time_idx * num_assets_ + asset_id;
    return parent_indices_[flat_idx];
  }

  // ============================================================================
  // TIME INDEX MANAGEMENT (Per-asset)
  // ============================================================================

  size_t get_time_idx(size_t asset_id) const { 
    return per_asset_time_idx_[asset_id]; 
  }
  
  size_t get_max_time_idx() const {
    return per_asset_time_idx_.empty() ? 0 : 
           *std::max_element(per_asset_time_idx_.begin(), per_asset_time_idx_.end());
  }

  // ============================================================================
  // METADATA ACCESSORS
  // ============================================================================

  size_t get_num_assets() const { return num_assets_; }
  size_t get_num_features() const { return num_features_; }
  size_t get_capacity() const { return capacity_per_asset_; }
  
  // ============================================================================
  // DATA EXPORT
  // ============================================================================
  
  // Get raw pointer for zero-copy export
  const float* get_data_ptr() const { return data_; }
  const size_t* get_parent_indices_ptr() const { return parent_indices_; }
  
  // Get specific row (asset × time)
  const float* get_row(size_t asset_id, size_t time_idx) const {
    const size_t row_offset = (time_idx * num_assets_ + asset_id) * num_features_;
    return data_ + row_offset;
  }
};

// ============================================================================
// DAILY FEATURE TENSOR - MULTI-LEVEL STORAGE FOR ONE DATE
// ============================================================================
// Contains all levels (L0, L1, L2, ...) for a single trading date
// Each level stores data for all assets with shared time indices

class DailyFeatureTensor {
private:
  LevelStorage levels_[LEVEL_COUNT]; // Storage for each level
  std::string date_;                 // Trading date identifier
  size_t num_assets_;                // Number of assets in this tensor

public:
  // ============================================================================
  // INITIALIZATION
  // ============================================================================

  DailyFeatureTensor(const std::string &date, size_t num_assets)
      : date_(date), num_assets_(num_assets) {
    // Initialize all levels with their respective capacities
    for (size_t level_idx = 0; level_idx < LEVEL_COUNT; ++level_idx) {
      levels_[level_idx].initialize(
          FIELDS_PER_LEVEL[level_idx],
          num_assets,
          MAX_ROWS_PER_LEVEL[level_idx]);
    }
  }

  // ============================================================================
  // LEVEL ACCESS
  // ============================================================================

  LevelStorage &get_level(size_t level_idx) { return levels_[level_idx]; }
  const LevelStorage &get_level(size_t level_idx) const { return levels_[level_idx]; }

  // ============================================================================
  // METADATA ACCESS
  // ============================================================================

  const std::string &get_date() const { return date_; }
  size_t get_num_assets() const { return num_assets_; }
};

// ============================================================================
// GLOBAL FEATURE STORE - DATE-SHARDED MULTI-ASSET MANAGER
// ============================================================================
// Top-level container managing all dates and assets
// Optimized for hot path: cached pointers, minimal lookups
// Thread-safe: Each asset has independent thread, no cross-asset contention

class GlobalFeatureStore {
private:
  // Date-sharded storage (ordered for chronological export)
  std::map<std::string, std::unique_ptr<DailyFeatureTensor>> daily_tensors_;
  mutable std::shared_mutex map_mutex_; // Protects map structure only
  
  size_t num_assets_;

  // Per-asset cached pointers (HOT PATH optimization)
  std::vector<DailyFeatureTensor*> current_tensors_;
  std::vector<std::string> current_dates_;

public:
  // ============================================================================
  // INITIALIZATION
  // ============================================================================

  explicit GlobalFeatureStore(size_t num_assets)
      : num_assets_(num_assets), 
        current_tensors_(num_assets, nullptr),
        current_dates_(num_assets) {}

  // ============================================================================
  // DATE SWITCHING (Called by main.cpp when asset moves to next date)
  // ============================================================================

  // Switch asset to new date - updates cached pointer
  void switch_date(size_t asset_id, const std::string &date) {
    assert(asset_id < num_assets_ && "Invalid asset_id");
    current_dates_[asset_id] = date;
    
    // Double-checked locking for tensor creation
    DailyFeatureTensor* tensor;
    {
      std::shared_lock lock(map_mutex_);
      auto it = daily_tensors_.find(date);
      if (it != daily_tensors_.end()) {
        tensor = it->second.get();
        current_tensors_[asset_id] = tensor;
        return;
      }
    }
    
    // Need to create new tensor
    {
      std::unique_lock lock(map_mutex_);
      auto it = daily_tensors_.find(date);
      if (it == daily_tensors_.end()) {
        auto new_tensor = std::make_unique<DailyFeatureTensor>(date, num_assets_);
        tensor = new_tensor.get();
        daily_tensors_.emplace(date, std::move(new_tensor));
      } else {
        tensor = it->second.get();
      }
    }
    
    current_tensors_[asset_id] = tensor;
  }
  
  // Legacy compatibility (redirects to switch_date)
  void set_current_date(size_t asset_id, const std::string &date,
                        [[maybe_unused]] const size_t *level_reserve_sizes = nullptr) {
    switch_date(asset_id, date);
  }

  // ============================================================================
  // HOT PATH: ULTRA-FAST PUSH (Called billions of times)
  // ============================================================================
  
  // Single-line push with automatic parent linking and time increment
  // No map lookup, no string comparison - just cached pointer dereference
  template <typename LevelData>
  inline void push(size_t level_idx, size_t asset_id, const LevelData* data) {
    DailyFeatureTensor* tensor = current_tensors_[asset_id];
    assert(tensor && "Call switch_date before push");
    
    LevelStorage& level = tensor->get_level(level_idx);
    
    // Auto-determine parent index from parent level
    size_t parent_idx = 0;
    if (level_idx > 0) {
      parent_idx = tensor->get_level(level_idx - 1).get_time_idx(asset_id);
    }
    
    level.push_row(asset_id, *data, parent_idx);
  }

  // ============================================================================
  // TENSOR ACCESS (For advanced use)
  // ============================================================================

  DailyFeatureTensor* get_tensor(const std::string &date) const {
    std::shared_lock lock(map_mutex_);
    auto it = daily_tensors_.find(date);
    return it != daily_tensors_.end() ? it->second.get() : nullptr;
  }
  
  const std::string& get_current_date(size_t asset_id) const {
    return current_dates_[asset_id];
  }

  // ============================================================================
  // EXPORT: HIERARCHICAL TENSOR WITH PARENT LINKING
  // ============================================================================
  
  // Export single date as [time × asset × all_level_features]
  // Features arranged as: [L0_features | L1_features | L2_features | ...]
  // Each row follows parent chain to link multi-level features
  std::vector<float> export_date_tensor(const std::string& date) const {
    DailyFeatureTensor* tensor = get_tensor(date);
    if (!tensor) return {};
    
    // Calculate dimensions
    const size_t max_time = tensor->get_level(L0_INDEX).get_max_time_idx();
    if (max_time == 0) return {};
    
    size_t total_features = 0;
    for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
      total_features += FIELDS_PER_LEVEL[lvl];
    }
    
    // Allocate output: [time × asset × total_features]
    std::vector<float> output(max_time * num_assets_ * total_features, 0.0f);
    
    // Export each asset independently
    for (size_t asset_id = 0; asset_id < num_assets_; ++asset_id) {
      const size_t asset_time = tensor->get_level(L0_INDEX).get_time_idx(asset_id);
      
      for (size_t t = 0; t < asset_time; ++t) {
        float* row_out = output.data() + (t * num_assets_ + asset_id) * total_features;
        size_t feature_offset = 0;
        
        // Follow parent chain across levels
        size_t parent_time_idx = t;
        for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
          const LevelStorage& level = tensor->get_level(lvl);
          const float* src = level.get_row(asset_id, parent_time_idx);
          const size_t num_features = FIELDS_PER_LEVEL[lvl];
          
          std::memcpy(row_out + feature_offset, src, num_features * sizeof(float));
          feature_offset += num_features;
          
          // Get parent index for next level
          if (lvl + 1 < LEVEL_COUNT) {
            parent_time_idx = level.get_parent_index(asset_id, parent_time_idx);
          }
        }
      }
    }
    
    return output;
  }

  // ============================================================================
  // METADATA ACCESS
  // ============================================================================

  size_t get_num_assets() const { return num_assets_; }
  
  size_t get_num_dates() const { 
    std::shared_lock lock(map_mutex_);
    return daily_tensors_.size(); 
  }
};

// ============================================================================
// AUTO-GENERATED: push_row TEMPLATE SPECIALIZATIONS
// ============================================================================
// Fully macro-driven generation - automatically scales to all levels
// Each specialization performs:
// 1. Check capacity and auto-expand if needed
// 2. Get per-asset time index (independent for each asset)
// 3. Compute flat array offset: (time × num_assets + asset) × num_features
// 4. Memcpy struct fields to destination
// 5. Record parent row index for hierarchical linking
// 6. Auto-increment per-asset time index

#define GENERATE_PUSH_ROW_SPECIALIZATION(level_name, level_num, fields)            \
  template <>                                                                      \
  inline void LevelStorage::push_row<Level##level_num##Data>(                      \
      size_t asset_id,                                                             \
      const Level##level_num##Data &data,                                          \
      size_t parent_row_idx) {                                                     \
    /* Auto-expand if this asset needs more capacity */                            \
    if (needs_expansion(asset_id)) [[unlikely]] {                                  \
      expand_capacity();                                                           \
    }                                                                              \
    /* Use per-asset time index */                                                 \
    const size_t time_idx = per_asset_time_idx_[asset_id];                         \
    const size_t row_offset = (time_idx * num_assets_ + asset_id) * num_features_; \
    float *dest = data_ + row_offset;                                              \
    const float *src = reinterpret_cast<const float *>(&data);                     \
    std::memcpy(dest, src, num_features_ * sizeof(float));                         \
    parent_indices_[time_idx * num_assets_ + asset_id] = parent_row_idx;           \
    /* Auto-increment this asset's time index */                                   \
    per_asset_time_idx_[asset_id]++;                                               \
  }

// Generate specializations for all levels
ALL_LEVELS(GENERATE_PUSH_ROW_SPECIALIZATION)
