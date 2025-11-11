#pragma once

#include "FeatureStoreConfig.hpp"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// ============================================================================
// FEATURE STORE CONFIGURATION
// ============================================================================
// Control tensor flush strategy:
// - true:  Flush unified daily tensor [T_L0, F_total, A] (GPU-friendly, single file)
// - false: Flush separate level tensors [T_L0, F0, A], [T_L1, F1, A], [T_L2, F2, A]
#define STORE_UNIFIED_DAILY_TENSOR false

// ============================================================================
// FEATURE STORE - Single class interface
// ============================================================================
// Design: [T][F][A] layout for optimal CS operations
// Lockfree sync: per-TS-core progress tracking
// ============================================================================

class GlobalFeatureStore {
private:
  // Tensor lifecycle states
  enum class TensorState : uint8_t {
    UNUSED = 0,  // Not in use, ready to allocate
    IN_USE = 1,  // Allocated to a date, TS/CS processing ongoing
    CS_DONE = 2, // CS processing complete, ready for IO thread to flush
    FLUSHING = 3 // IO thread is flushing, will be UNUSED soon
  };

  // Per-date storage
  struct DayData {
    feature_storage_t *data[LEVEL_COUNT] = {nullptr}; // [level][T][F][A] stored as _Float16
    size_t **ts_progress = nullptr;                   // [level][core] - non-atomic, each core writes its own
    std::atomic<TensorState> state{TensorState::IN_USE}; // Lifecycle state

    void allocate(size_t num_assets, size_t num_cores) {
      // Allocate feature data
      for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
        const size_t T = MAX_ROWS_PER_LEVEL[lvl];
        const size_t F = FIELDS_PER_LEVEL[lvl];
        const size_t A = num_assets;

        if (!data[lvl]) {
          const size_t total_elements = T * F * A;
          const size_t total_bytes = total_elements * sizeof(feature_storage_t);
          const size_t aligned_bytes = ((total_bytes + 63) / 64) * 64;
          data[lvl] = static_cast<feature_storage_t *>(std::aligned_alloc(64, aligned_bytes));
          assert(data[lvl] && "aligned_alloc failed");
        }
      }

      // Allocate ts_progress array
      if (!ts_progress) {
        ts_progress = new size_t *[LEVEL_COUNT];
        for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
          ts_progress[lvl] = new size_t[num_cores]();
        }
      }
    }

    // Release only the data arrays, keep metadata (ts_progress, state)
    void release_data() {
      for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
        if (data[lvl]) {
          std::free(data[lvl]);
          data[lvl] = nullptr;
        }
      }
    }

    ~DayData() {
      release_data();
      if (ts_progress) {
        for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
          delete[] ts_progress[lvl];
        }
        delete[] ts_progress;
      }
    }
  };

  // ===== Core Data Structures (No Pool, Direct Map) =====
  std::map<std::string, DayData*> date_to_daydata_; // Date string -> DayData pointer
  mutable std::mutex map_mutex_;                     // Protects date_to_daydata_

  // ===== Worker Caches (Lock-free Optimization) =====
  mutable std::vector<std::string> ts_cache_date_; // [worker_id] -> current date string
  mutable std::vector<DayData *> ts_cache_data_;   // [worker_id] -> current DayData*
  mutable std::string cs_cache_date_;              // Current CS worker date string
  mutable DayData *cs_cache_data_ = nullptr;       // Current CS worker DayData*

  // ===== Configuration (Immutable) =====
  const size_t num_assets_;
  const size_t num_cores_;
  std::string output_dir_ = "./output/features";

  // ===== Internal Helpers =====
  // Flush tensor to disk (called by IO worker only, no concurrent access)
  void flush_to_disk(const std::string &date_str, DayData *day) {
    if (!day || date_str.size() != 8)
      return;

    // Validate data pointers before any operations
    for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
      if (!day->data[lvl]) {
        std::cerr << "ERROR: day->data[" << lvl << "] is null for date " << date_str << std::endl;
        return;
      }
    }

    // Create directory: output/features/YYYY/MM/DD
    std::string year = date_str.substr(0, 4);
    std::string month = date_str.substr(4, 2);
    std::string day_str = date_str.substr(6, 2);
    std::string out_dir = output_dir_ + "/" + year + "/" + month + "/" + day_str;
    std::filesystem::create_directories(out_dir);

    const size_t T0 = MAX_ROWS_PER_LEVEL[0];
    const size_t T1 = MAX_ROWS_PER_LEVEL[1];
    const size_t T2 = MAX_ROWS_PER_LEVEL[2];
    const size_t F0 = FIELDS_PER_LEVEL[0];
    const size_t F1 = FIELDS_PER_LEVEL[1];
    const size_t F2 = FIELDS_PER_LEVEL[2];
    const size_t A = num_assets_;

#if STORE_UNIFIED_DAILY_TENSOR
    // Unified mode: [T_L0, F_total, A] single file
    const size_t F_total = F0 + F1 + F2;
    std::string tensor_file = out_dir + "/features.bin";
    std::ofstream ofs(tensor_file, std::ios::binary);
    if (!ofs) {
      std::cerr << "Failed to open unified tensor file: " << tensor_file << std::endl;
      return;
    }

    // Write header: [T, F, A]
    ofs.write(reinterpret_cast<const char *>(&T0), sizeof(size_t));
    ofs.write(reinterpret_cast<const char *>(&F_total), sizeof(size_t));
    ofs.write(reinterpret_cast<const char *>(&A), sizeof(size_t));

    // Get link feature offsets from L0
    const size_t link_to_L1_offset = L0_FieldOffset::_link_to_L1;
    const size_t link_to_L2_offset = L0_FieldOffset::_link_to_L2;

    // Write data: for each L0 time t0
    for (size_t t0 = 0; t0 < T0; ++t0) {
      // Read L1/L2 indices from L0 link features (stored as uint16_t, reinterpret as index)
      // Use first asset's link (all assets share same time mapping)
      const size_t t1 = static_cast<size_t>(day->data[0][t0 * F0 * A + link_to_L1_offset * A]);
      const size_t t2 = static_cast<size_t>(day->data[0][t0 * F0 * A + link_to_L2_offset * A]);

      // L0 features
      for (size_t f = 0; f < F0; ++f) {
        ofs.write(reinterpret_cast<const char *>(day->data[0] + t0 * F0 * A + f * A), A * sizeof(feature_storage_t));
      }

      // L1 features (upsampled via link)
      for (size_t f = 0; f < F1; ++f) {
        ofs.write(reinterpret_cast<const char *>(day->data[1] + t1 * F1 * A + f * A), A * sizeof(feature_storage_t));
      }

      // L2 features (upsampled via link)
      for (size_t f = 0; f < F2; ++f) {
        ofs.write(reinterpret_cast<const char *>(day->data[2] + t2 * F2 * A + f * A), A * sizeof(feature_storage_t));
      }
    }
#else
    // Separate mode: 3 level files (link already in L0 features, no separate metadata file needed)
    // L0 tensor: [T0, F0, A] (includes _link_to_L1 and _link_to_L2 features)
    {
      std::string l0_file = out_dir + "/features_L0.bin";
      std::ofstream ofs(l0_file, std::ios::binary);
      if (!ofs) {
        std::cerr << "Failed to open L0 file: " << l0_file << std::endl;
        return;
      }
      ofs.write(reinterpret_cast<const char *>(&T0), sizeof(size_t));
      ofs.write(reinterpret_cast<const char *>(&F0), sizeof(size_t));
      ofs.write(reinterpret_cast<const char *>(&A), sizeof(size_t));
      for (size_t t = 0; t < T0; ++t) {
        for (size_t f = 0; f < F0; ++f) {
          ofs.write(reinterpret_cast<const char *>(day->data[0] + t * F0 * A + f * A), A * sizeof(feature_storage_t));
        }
      }
    }

    // L1 tensor: [T1, F1, A]
    {
      std::string l1_file = out_dir + "/features_L1.bin";
      std::ofstream ofs(l1_file, std::ios::binary);
      if (!ofs) {
        std::cerr << "Failed to open L1 file: " << l1_file << std::endl;
        return;
      }
      ofs.write(reinterpret_cast<const char *>(&T1), sizeof(size_t));
      ofs.write(reinterpret_cast<const char *>(&F1), sizeof(size_t));
      ofs.write(reinterpret_cast<const char *>(&A), sizeof(size_t));
      for (size_t t = 0; t < T1; ++t) {
        for (size_t f = 0; f < F1; ++f) {
          ofs.write(reinterpret_cast<const char *>(day->data[1] + t * F1 * A + f * A), A * sizeof(feature_storage_t));
        }
      }
    }

    // L2 tensor: [T2, F2, A]
    {
      std::string l2_file = out_dir + "/features_L2.bin";
      std::ofstream ofs(l2_file, std::ios::binary);
      if (!ofs) {
        std::cerr << "Failed to open L2 file: " << l2_file << std::endl;
        return;
      }
      ofs.write(reinterpret_cast<const char *>(&T2), sizeof(size_t));
      ofs.write(reinterpret_cast<const char *>(&F2), sizeof(size_t));
      ofs.write(reinterpret_cast<const char *>(&A), sizeof(size_t));
      for (size_t t = 0; t < T2; ++t) {
        for (size_t f = 0; f < F2; ++f) {
          ofs.write(reinterpret_cast<const char *>(day->data[2] + t * F2 * A + f * A), A * sizeof(feature_storage_t));
        }
      }
    }
#endif
  }

  // Allocate or find tensor from pool (internal function, called by get_cached_data on cache miss)
  // NOTE: This function maintains both TS and CS worker caches
  //       Does NOT check cache - cache checking is done by get_cached_data
  DayData *get_or_create_daydata(const std::string &date, int worker_id = -1) {
    std::lock_guard<std::mutex> lock(map_mutex_);

    // Check if date already exists
    auto it = date_to_daydata_.find(date);
    if (it != date_to_daydata_.end()) {
      DayData *result = it->second;
      
      // Update appropriate cache based on worker_id
      if (worker_id >= 0 && worker_id < static_cast<int>(ts_cache_date_.size())) {
        ts_cache_date_[worker_id] = date;
        ts_cache_data_[worker_id] = result;
      } else if (worker_id < 0) {
        cs_cache_date_ = date;
        cs_cache_data_ = result;
      }
      
      return result;
    }

    // Date doesn't exist, create new DayData
    DayData *day = new DayData();
    day->allocate(num_assets_, num_cores_);
    day->state.store(TensorState::IN_USE, std::memory_order_release);
    date_to_daydata_[date] = day;

    // Update appropriate cache
    if (worker_id >= 0 && worker_id < static_cast<int>(ts_cache_date_.size())) {
      ts_cache_date_[worker_id] = date;
      ts_cache_data_[worker_id] = day;
    } else if (worker_id < 0) {
      cs_cache_date_ = date;
      cs_cache_data_ = day;
    }

    return day;
  }

public:
  GlobalFeatureStore(size_t num_assets, size_t num_cores, const std::string &output_dir = "")
      : num_assets_(num_assets), num_cores_(num_cores) {

    if (!output_dir.empty()) {
      output_dir_ = output_dir;
      // Auto wipe and create
      if (std::filesystem::exists(output_dir_)) {
        std::cout << "Wiping and creating output directory: " << output_dir_ << std::endl;
        std::filesystem::remove_all(output_dir_);
      }
      std::filesystem::create_directories(output_dir_);
    }

    size_t bytes_per_day = 0;
    size_t bytes_per_level[LEVEL_COUNT] = {0};
    for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
      bytes_per_level[lvl] = MAX_ROWS_PER_LEVEL[lvl] * num_assets * FIELDS_PER_LEVEL[lvl] * sizeof(feature_storage_t);
      bytes_per_day += bytes_per_level[lvl];
    }

    // Calculate storage sizes
    const size_t total_features = FIELDS_PER_LEVEL[0] + FIELDS_PER_LEVEL[1] + FIELDS_PER_LEVEL[2];

    std::cout << "\n=== Feature Store (Dynamic Allocation) ===\n";
    std::cout << "Assets: " << num_assets << " | Workers(TS): " << num_cores << "\n";

    std::cout << "Level  Features   Time×Asset    PerDay(MB)  Description\n";
    std::cout << "-----  --------  -----------  -----------  -----------\n";

    const char *level_desc[] = {"1s tick", "1min bar", "1h bar"};
    for (size_t lvl = 0; lvl < LEVEL_COUNT; ++lvl) {
      const size_t T = MAX_ROWS_PER_LEVEL[lvl];
      const size_t F = FIELDS_PER_LEVEL[lvl];
      const double per_day_mb = bytes_per_level[lvl] / (1024.0 * 1024.0);

      printf("  L%zu   %4zu       %5zu×%-4zu      %8.2f  %s\n",
             lvl, F, T, num_assets, per_day_mb, level_desc[lvl]);
    }

    std::cout << "-----  --------  -----------  -----------  -----------\n";
    printf("Total  %4zu                        %8.1f  per daily tensor\n",
           total_features,
           bytes_per_day / (1024.0 * 1024.0));
    std::cout << "=================================\n";

    // Initialize per-worker cache
    ts_cache_date_.resize(num_cores);
    ts_cache_data_.resize(num_cores, nullptr);
  }

  ~GlobalFeatureStore() {
    // Clean up all DayData objects
    for (auto &[date, day] : date_to_daydata_) {
      delete day;
    }
  }

  // ===== Cache Layer (Public Access Point) =====
  // Get DayData* from cache (for metadata access: ts_progress, etc.)
  // Maintains both TS and CS worker caches transparently
  DayData *get_cached_data(const std::string &date, int worker_id = -1) const {
    // Fast path 1: TS worker cache (worker_id >= 0)
    if (worker_id >= 0 && worker_id < static_cast<int>(ts_cache_date_.size())) {
      if (ts_cache_date_[worker_id] == date && ts_cache_data_[worker_id]) {
        return ts_cache_data_[worker_id];
      }
    }

    // Fast path 2: CS worker cache (worker_id < 0)
    if (worker_id < 0 && cs_cache_date_ == date && cs_cache_data_) {
      return cs_cache_data_;
    }

    // Slow path: get or create from map (need const_cast as it may create new date)
    return const_cast<GlobalFeatureStore *>(this)->get_or_create_daydata(date, worker_id);
  }

  // Get feature_storage_t* from cache (for feature data read/write)
  // Delegates to get_cached_data for cache consistency
  feature_storage_t *get_cached_ptr(const std::string &date, size_t level_idx, int worker_id = -1) {
    DayData *day = get_cached_data(date, worker_id);
    return day->data[level_idx];
  }

  // ===== TS Worker Interface =====
  void ts_mark_done(const std::string &date, size_t level_idx, size_t core_id, size_t l0_time_index) {
    DayData *day = get_cached_data(date, core_id);
    if (!day) {
      std::cerr << "FATAL: Failed to allocate tensor for " << date << "\n";
      std::exit(1);
    }
    day->ts_progress[level_idx][core_id] = l0_time_index + 1;
  }

  void ts_write_link(const std::string &date, size_t l0_t, size_t asset_idx,
                     size_t link_feature_offset, _Float16 link_value) {
    feature_storage_t *base = get_cached_ptr(date, 0, -1);
    if (!base) {
      std::cerr << "FATAL: Failed to write link for " << date << "\n";
      std::exit(1);
    }
    const size_t F0 = FIELDS_PER_LEVEL[0];
    const size_t A = num_assets_;
    base[l0_t * F0 * A + link_feature_offset * A + asset_idx] = link_value;
  }

  // ===== CS Worker Interface =====
  bool cs_check_ready(const std::string &date, size_t level_idx, size_t l0_time_index) const {
    DayData *day = get_cached_data(date, -1);
    if (!day)
      return false;

    // Check ts_progress (lock-free, each TS core only writes its own slot)
    for (size_t core_id = 0; core_id < num_cores_; ++core_id) {
      if (day->ts_progress[level_idx][core_id] <= l0_time_index) {
        return false;
      }
    }
    return true;
  }

  void cs_mark_complete(const std::string &date) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = date_to_daydata_.find(date);
    if (it != date_to_daydata_.end()) {
      it->second->state.store(TensorState::CS_DONE, std::memory_order_release);
    }
  }

  // ===== Query Interface =====
  size_t query_F(size_t level_idx) const { return FIELDS_PER_LEVEL[level_idx]; }
  size_t query_A() const { return num_assets_; }
  size_t query_T(size_t level_idx) const { return MAX_ROWS_PER_LEVEL[level_idx]; }
  size_t query_num_assets() const { return num_assets_; }
  size_t query_num_dates() const {
    std::lock_guard<std::mutex> lock(map_mutex_);
    return date_to_daydata_.size();
  }

  // Debug: get pool status as a single line string
  std::string debug_get_pool_status() const {
    std::lock_guard<std::mutex> lock(map_mutex_);

    std::string result = " [";
    bool first = true;

    for (const auto &[date, day] : date_to_daydata_) {
      if (!first)
        result += ", ";
      first = false;

      const char *state_str = "?";
      auto state = day->state.load(std::memory_order_acquire);
      switch (state) {
      case TensorState::UNUSED:
        state_str = "U";
        break;
      case TensorState::IN_USE:
        state_str = "I";
        break;
      case TensorState::CS_DONE:
        state_str = "D";
        break;
      case TensorState::FLUSHING:
        state_str = "F";
        break;
      }

      result += date + ":" + state_str;
    }

    result += "]";
    return result;
  };

  // ===== IO Worker Interface =====
  // Flush the oldest CS_DONE tensor to disk (returns true if flushed, false if none ready)
  bool io_flush_once() {
    // Find oldest (first in map order) CS_DONE tensor
    std::string date_to_flush;
    DayData *day_to_flush = nullptr;

    {
      std::lock_guard<std::mutex> lock(map_mutex_);
      // std::map is ordered by key (date string), so iterate in order
      for (const auto &[date, day] : date_to_daydata_) {
        if (day->state.load(std::memory_order_acquire) == TensorState::CS_DONE) {
          date_to_flush = date;
          day_to_flush = day;
          break; // Found oldest CS_DONE
        }
      }
    }

    if (!day_to_flush) {
      return false; // No CS_DONE tensor found
    }

    // Mark FLUSHING
    day_to_flush->state.store(TensorState::FLUSHING, std::memory_order_release);

    // Flush to disk
    flush_to_disk(date_to_flush, day_to_flush);

    //// Release only the data arrays, keep DayData structure and ts_progress
    //day_to_flush->release_data();

    // Mark as UNUSED (but keep in map, memory can be reclaimed by OS)
    day_to_flush->state.store(TensorState::UNUSED, std::memory_order_release);

    return true;
  }

  // ===== Configuration Interface =====
  void config_set_output_dir(const std::string &dir) {
    output_dir_ = dir;
  }
};

// ============================================================================
// DATA ACCESS MACROS
// ============================================================================
// Note: All macros work with feature_storage_t (_Float16)
// Automatic conversion between float and _Float16 (like float <-> double)

// TS worker: write features for asset a at time t (src is feature_storage_t*)
// Optional: pass worker_id for cache optimization
#define TS_WRITE_FEATURES(store, date, level_idx, t, a, f_start, f_end, src, ...)      \
  do {                                                                                 \
    feature_storage_t *base = (store)->get_cached_ptr(date, level_idx, ##__VA_ARGS__); \
    const size_t F = (store)->query_F(level_idx);                                      \
    const size_t A = (store)->query_A();                                               \
    const size_t base_offset = (t) * F * A + (a);                                      \
    for (size_t f = (f_start); f < (f_end); ++f) {                                     \
      base[base_offset + f * A] = (src)[f];                                            \
    }                                                                                  \
  } while (0)

// CS worker: read all assets for feature f at time t (returns feature_storage_t*)
#define CS_READ_ALL_ASSETS(store, date, level_idx, t, f) \
  ((store)->get_cached_ptr(date, level_idx, -1) + (t) * (store)->query_F(level_idx) * (store)->query_A() + (f) * (store)->query_A())

// CS worker: write all assets for feature f at time t (src is feature_storage_t*)
#define CS_WRITE_ALL_ASSETS(store, date, level_idx, t, f, src, count)                                                                           \
  std::memcpy((store)->get_cached_ptr(date, level_idx, -1) + (t) * (store)->query_F(level_idx) * (store)->query_A() + (f) * (store)->query_A(), \
              (src), (count) * sizeof(feature_storage_t))

// Read single value (returns feature_storage_t)
#define READ_FEATURE(store, date, level_idx, t, f, a) \
  ((store)->get_cached_ptr(date, level_idx, -1)[(t) * (store)->query_F(level_idx) * (store)->query_A() + (f) * (store)->query_A() + (a)])

// Write single value (value is feature_storage_t)
#define WRITE_FEATURE(store, date, level_idx, t, f, a, value)                                                                                        \
  do {                                                                                                                                               \
    (store)->get_cached_ptr(date, level_idx, -1)[(t) * (store)->query_F(level_idx) * (store)->query_A() + (f) * (store)->query_A() + (a)] = (value); \
  } while (0)

// Write link feature (L0 only): map L0 time to L1/L2 time
// link_feature_offset: L0_FieldOffset::_link_to_L1 or _link_to_L2
// link_value: L1 or L2 time index (stored as _Float16, auto-converted from size_t)
#define WRITE_LINK_FEATURE(store, date, l0_t, asset_idx, link_feature_offset, link_value)                  \
  do {                                                                                                     \
    (store)->ts_write_link(date, l0_t, asset_idx, link_feature_offset, static_cast<_Float16>(link_value)); \
  } while (0)
