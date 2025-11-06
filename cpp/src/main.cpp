#include "codec/L2_DataType.hpp"
#include "codec/binary_decoder_L2.hpp"
#include "codec/binary_encoder_L2.hpp"
#include "codec/json_config.hpp"
#include "features/backend/FeatureStore.hpp"
#include "lob/LimitOrderBook.hpp"
#include "misc/affinity.hpp"
#include "misc/logging.hpp"
#include "misc/progress_parallel.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <regex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

// ============================================================================
// L2数据处理架构 - 两阶段并行处理
// ============================================================================
//
// 【核心设计】
//   数据结构: SharedState 统一管理所有共享状态
//     - assets[]: 所有资产信息（元数据 + 每日统计 + 文件路径 + 状态位图）
//     - all_dates[]: 全局交易日序列（用于横截面因子同步）
//
//   Phase 1 (Encoding): Asset并行，Date乱序
//     - Worker领取asset，shuffle日期顺序以分散RAR访问压力
//     - RAR锁（阻塞模式）：确保同一压缩包不被并发解压
//     - 记录统计信息到 asset.date_info[]，零额外扫描
//
//   Phase 2 (Analysis): Date-first遍历，横截面同步
//     - 所有worker按 all_dates[] 顺序同步推进
//     - 每个date处理完成后可插入横截面因子计算
//     - 无锁读取：所有路径/统计信息已在Phase 1缓存
//
// 【数据流】
//   RAR → CSV → L2结构 → Zstd压缩 → Binary文件
//                ↓ (统计信息记录到 date_info)
//   Binary文件 → 解压解码 → Order Book → 特征提取 → FeatureStore
//
// 【Key Insights】
//   1. 零重复扫描: Encoding时记录order_count/文件路径，Analysis直接读取
//   2. 路径缓存: 所有路径初始化时生成一次，存储在 date_info.temp_dir
//   3. 类型缓存: exchange_type 构造时推导一次，避免字符串解析
//   4. 状态位图: date_info.encoded/analyzed 支持断点续传和精确追踪
//   5. 负载均衡: 使用encoding时累积的order_count，无需预扫描
//   6. 横截面准备: all_dates[] 全局同步，预留 cross_sectional_cache
//
// 【线程安全】
//   - Encoding: 每个worker只写自己的asset，relaxed无锁
//   - Analysis: 只读共享状态，零锁开销
//   - RAR解压: 按archive_path加锁，细粒度并发
//
// 【性能优化】
//   - 跳过已编码文件 + 断点续传支持
//   - 日期shuffle分散RAR访问热点
//   - CPU亲和性减少cache miss
//   - Zstd解压速度 1300+ MB/s
//
// ============================================================================
// CONFIGURATION SECTION
// ============================================================================
// Modify these constants to adapt to different environments or data suppliers
namespace Config {
// Archive settings - modify these for different compression formats
constexpr const char *ARCHIVE_EXTENSION = ".rar"; // Archive file extension (.rar/.7z/.zip)
constexpr const char *ARCHIVE_TOOL = "unrar";     // Archive extraction tool (unrar/7z/unzip)
constexpr const char *ARCHIVE_EXTRACT_CMD = "x";  // Extract command (x for unrar, x for 7z)

// File extensions and names - standard CSV filenames from data supplier
constexpr const char *BIN_EXTENSION = ".bin";
// CSV filenames defined here for documentation and potential future use
// Currently the encoder auto-detects these files, but explicit names reserved for future API changes
[[maybe_unused]] constexpr const char *CSV_MARKET_DATA = "行情.csv";    // Market snapshot CSV filename
[[maybe_unused]] constexpr const char *CSV_TICK_TRADE = "逐笔成交.csv"; // Tick-by-tick trade CSV filename
[[maybe_unused]] constexpr const char *CSV_TICK_ORDER = "逐笔委托.csv"; // Tick-by-tick order CSV filename

// Path settings - modify these for your environment
constexpr const char *DEFAULT_CONFIG_FILE = "../../../../config/config.json";
constexpr const char *DEFAULT_STOCK_INFO_FILE = "../../../../config/daily_holding/stock_info_test.json";
constexpr const char *DEFAULT_L2_ARCHIVE_BASE = "/mnt/dev/sde/A_stock/L2";
constexpr const char *DEFAULT_TEMP_DIR = "../../../../output/database";

// Processing settings - modify for different behaviors
constexpr bool CLEANUP_AFTER_PROCESSING = false; // Clean up temp files after processing (saves disk space)
constexpr bool SKIP_EXISTING_BINARIES = true;    // Skip extraction/encoding if binary files exist (faster rerun)

} // namespace Config

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

namespace Utils {
inline std::string generate_archive_path(const std::string &base_dir, const std::string &date_str) {
  return base_dir + "/" + date_str.substr(0, 4) + "/" + date_str.substr(0, 6) + "/" + date_str + Config::ARCHIVE_EXTENSION;
}

inline std::string generate_temp_asset_dir(const std::string &temp_dir, const std::string &date_str, const std::string &asset_code) {
  return temp_dir + "/" + date_str.substr(0, 4) + "/" + date_str.substr(4, 2) + "/" + date_str.substr(6, 2) + "/" + asset_code;
}

inline bool is_leap_year(int year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

inline int get_days_in_month(int year, int month) {
  if (month == 2) {
    return is_leap_year(year) ? 29 : 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) {
    return 30;
  }
  return 31;
}

inline std::vector<std::string> generate_date_range(const std::string &start_date, const std::string &end_date, const std::string &l2_archive_base) {
  std::vector<std::string> dates;

  std::regex date_regex(R"((\d{4})_(\d{2})_(\d{2}))");
  std::smatch start_match, end_match;

  if (!std::regex_match(start_date, start_match, date_regex) || !std::regex_match(end_date, end_match, date_regex)) {
    return dates;
  }

  const int start_year = std::stoi(start_match[1]);
  const int start_mon = std::stoi(start_match[2]);
  const int start_day = std::stoi(start_match[3]);
  const int end_year = std::stoi(end_match[1]);
  const int end_mon = std::stoi(end_match[2]);
  const int end_day = std::stoi(end_match[3]);

  const bool has_archive_base = std::filesystem::exists(l2_archive_base);

  for (int year = start_year; year <= end_year; ++year) {
    const int mon_start = (year == start_year) ? start_mon : 1;
    const int mon_end = (year == end_year) ? end_mon : 12;

    for (int month = mon_start; month <= mon_end; ++month) {
      const int day_start = (year == start_year && month == start_mon) ? start_day : 1;
      const int day_end = (year == end_year && month == end_mon) ? end_day : get_days_in_month(year, month);

      for (int day = day_start; day <= day_end; ++day) {
        char date_buf[9];
        snprintf(date_buf, sizeof(date_buf), "%04d%02d%02d", year, month, day);
        const std::string date_str(date_buf);

        // If archive base exists, only include dates with existing archives
        // If not, include all dates (will be filtered in encoding/analysis phase)
        if (!has_archive_base || std::filesystem::exists(generate_archive_path(l2_archive_base, date_str))) {
          dates.push_back(date_str);
        }
      }
    }
  }

  return dates;
}
} // namespace Utils

// ============================================================================
// RAR LOCK MANAGER
// ============================================================================
// Manages per-archive locks to prevent concurrent extraction from same RAR

class RarLockManager {
  inline static std::mutex map_mutex;
  inline static std::unordered_map<std::string, std::unique_ptr<std::mutex>> locks;

public:
  static std::mutex *get_or_create_lock(const std::string &archive_path) {
    std::lock_guard<std::mutex> lock(map_mutex);
    if (locks.find(archive_path) == locks.end()) {
      locks[archive_path] = std::make_unique<std::mutex>();
    }
    return locks[archive_path].get();
  }
};

// ============================================================================
// SHARED STATE - All assets and global synchronization
// ============================================================================

struct AssetInfo {
  // ===== Per-date information =====
  struct DateInfo {
    size_t order_count{0};      // Order count (written during encoding)
    uint8_t encoded{0};         // 0=not encoded, 1=encoded
    uint8_t analyzed{0};        // 0=not analyzed, 1=analyzed
    std::string temp_dir;       // Cached temp directory path
    std::string snapshots_file; // Full path to snapshots binary file
    std::string orders_file;    // Full path to orders binary file

    bool has_binaries() const {
      return !snapshots_file.empty() || !orders_file.empty();
    }
  };

  // ===== Immutable metadata =====
  size_t asset_id;
  std::string asset_code;
  std::string asset_name;
  std::vector<std::string> dates;
  L2::ExchangeType exchange_type; // Cached exchange type

  // ===== Per-date data =====
  std::vector<DateInfo> date_info;

  // ===== Analysis assignment =====
  int assigned_worker_id{-1};

  // Constructor
  AssetInfo(size_t id, std::string code, std::string name, std::vector<std::string> ds)
      : asset_id(id),
        asset_code(std::move(code)),
        asset_name(std::move(name)),
        dates(std::move(ds)),
        exchange_type(L2::infer_exchange_type(asset_code)),
        date_info(dates.size()) {}

  // Initialize paths (called once after construction)
  void init_paths(const std::string &temp_dir_base) {
    for (size_t i = 0; i < dates.size(); ++i) {
      date_info[i].temp_dir = Utils::generate_temp_asset_dir(temp_dir_base, dates[i], asset_code);
    }
  }

  // Scan existing binary files (for resume support)
  void scan_existing_binaries() {
    for (size_t i = 0; i < dates.size(); ++i) {
      auto &di = date_info[i];
      if (!std::filesystem::exists(di.temp_dir))
        continue;

      for (const auto &entry : std::filesystem::directory_iterator(di.temp_dir)) {
        const std::string filename = entry.path().filename().string();
        if (filename.starts_with(asset_code + "_snapshots_") && filename.ends_with(Config::BIN_EXTENSION)) {
          di.snapshots_file = entry.path().string();
        } else if (filename.starts_with(asset_code + "_orders_") && filename.ends_with(Config::BIN_EXTENSION)) {
          di.orders_file = entry.path().string();
        }
      }

      if (di.has_binaries()) {
        di.encoded = 1;
      }
    }
  }

  // Statistics
  size_t get_total_order_count() const {
    size_t total = 0;
    for (const auto &di : date_info)
      total += di.order_count;
    return total;
  }

  size_t get_encoded_count() const {
    size_t count = 0;
    for (const auto &di : date_info)
      count += di.encoded;
    return count;
  }

  size_t get_analyzed_count() const {
    size_t count = 0;
    for (const auto &di : date_info)
      count += di.analyzed;
    return count;
  }
};

struct SharedState {
  // ===== Asset-level information =====
  std::vector<AssetInfo> assets;

  // ===== Global date sequence =====
  std::vector<std::string> all_dates; // Sorted unique trading dates

  // ===== Future: Cross-sectional synchronization =====
  // std::vector<std::barrier<>> date_barriers;
  // std::vector<std::mutex> date_locks;
  // std::unordered_map<std::string, CrossSectionalData> cross_sectional_cache;

  SharedState() = default;

  // Initialize all_dates from assets
  void finalize_dates() {
    std::set<std::string> unique_dates_set;
    for (const auto &asset : assets) {
      unique_dates_set.insert(asset.dates.begin(), asset.dates.end());
    }
    all_dates.assign(unique_dates_set.begin(), unique_dates_set.end());
  }

  // Initialize all asset paths
  void init_paths(const std::string &temp_dir_base) {
    for (auto &asset : assets) {
      asset.init_paths(temp_dir_base);
    }
  }

  // Scan all existing binaries (for resume)
  void scan_all_existing_binaries() {
    for (auto &asset : assets) {
      asset.scan_existing_binaries();
    }
  }

  // Statistics
  size_t total_encoded_dates() const {
    size_t total = 0;
    for (const auto &asset : assets)
      total += asset.get_encoded_count();
    return total;
  }

  size_t total_orders() const {
    size_t total = 0;
    for (const auto &asset : assets)
      total += asset.get_total_order_count();
    return total;
  }
};

// ============================================================================
// ENCODING FUNCTIONS
// ============================================================================

namespace Encoding {
// Extract, parse CSV, and encode to binary files
// Returns true on success and populates date_info with file paths
inline bool extract_and_encode(const std::string &archive_path,
                               const std::string &asset_code,
                               const std::string &date_str,
                               const std::string &temp_dir,
                               L2::BinaryEncoder_L2 &encoder,
                               AssetInfo::DateInfo &date_info) {
  // Acquire lock (blocking)
  std::mutex *archive_lock = RarLockManager::get_or_create_lock(archive_path);
  std::lock_guard<std::mutex> lock(*archive_lock);

  // Extract from archive
  const std::string temp_extract_dir = temp_dir + "/tmp_" + asset_code;
  std::filesystem::create_directories(temp_extract_dir);

  const std::string archive_name = std::filesystem::path(archive_path).stem().string();
  const std::string asset_path_in_archive = archive_name + "/" + asset_code + "/*";
  const std::string command = std::string(Config::ARCHIVE_TOOL) + " " +
                              std::string(Config::ARCHIVE_EXTRACT_CMD) + " \"" +
                              archive_path + "\" \"" + asset_path_in_archive + "\" \"" +
                              temp_extract_dir + "/\" -y > /dev/null 2>&1";

  if (std::system(command.c_str()) != 0) {
    std::filesystem::remove_all(temp_extract_dir);
    return false;
  }

  const std::string extracted_dir = temp_extract_dir + "/" + date_str + "/" + asset_code;
  if (!std::filesystem::exists(extracted_dir)) {
    std::filesystem::remove_all(temp_extract_dir);
    return false;
  }

  // Move to final location
  std::filesystem::create_directories(std::filesystem::path(date_info.temp_dir).parent_path());

  // Remove target if exists to avoid rename failure
  if (std::filesystem::exists(date_info.temp_dir)) {
    std::filesystem::remove_all(date_info.temp_dir);
  }

  std::filesystem::rename(extracted_dir, date_info.temp_dir);
  std::filesystem::remove_all(temp_extract_dir);

  // Parse and encode
  std::vector<L2::Snapshot> snapshots;
  std::vector<L2::Order> orders;
  if (!encoder.process_stock_data(date_info.temp_dir, date_info.temp_dir, asset_code, &snapshots, &orders)) {
    return false;
  }

  // Record order count
  date_info.order_count = orders.size();

  // Scan for generated binary files
  for (const auto &entry : std::filesystem::directory_iterator(date_info.temp_dir)) {
    const std::string path = entry.path().string();
    if (path.ends_with(".csv")) {
      std::filesystem::remove(entry.path()); // Clean up CSV
    } else if (path.ends_with(Config::BIN_EXTENSION)) {
      const std::string filename = entry.path().filename().string();
      if (filename.starts_with(asset_code + "_snapshots_")) {
        date_info.snapshots_file = path;
      } else if (filename.starts_with(asset_code + "_orders_")) {
        date_info.orders_file = path;
      }
    }
  }

  return true;
}
} // namespace Encoding

// ============================================================================
// ANALYSIS FUNCTIONS
// ============================================================================

namespace Analysis {
inline size_t process_binary_files(const AssetInfo::DateInfo &date_info,
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
} // namespace Analysis

// ============================================================================
// PHASE 1: ENCODING WORKER
// ============================================================================

void encoding_worker(SharedState &state, std::vector<size_t> &asset_id_queue, std::mutex &queue_mutex, const std::string &l2_archive_base, const std::string &temp_dir, unsigned int core_id, misc::ProgressHandle progress_handle) {
  static thread_local bool affinity_set = false;
  if (!affinity_set && misc::Affinity::supported()) {
    affinity_set = misc::Affinity::pin_to_core(core_id);
  }

  L2::BinaryEncoder_L2 encoder(L2::DEFAULT_ENCODER_SNAPSHOT_SIZE, L2::DEFAULT_ENCODER_ORDER_SIZE);
  L2::BinaryDecoder_L2 decoder(L2::DEFAULT_ENCODER_SNAPSHOT_SIZE, L2::DEFAULT_ENCODER_ORDER_SIZE);

  while (true) {
    size_t asset_id;

    // Get an asset
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      if (asset_id_queue.empty()) {
        break;
      }
      asset_id = asset_id_queue.back();
      asset_id_queue.pop_back();
    }

    AssetInfo &asset = state.assets[asset_id];
    progress_handle.set_label(asset.asset_code + " (" + asset.asset_name + ")");

    // Shuffle date indices to spread RAR access
    std::vector<size_t> date_indices(asset.dates.size());
    std::iota(date_indices.begin(), date_indices.end(), 0);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(date_indices.begin(), date_indices.end(), g);

    // Process all dates for this asset
    for (size_t i = 0; i < date_indices.size(); ++i) {
      const size_t date_idx = date_indices[i];
      const std::string &date_str = asset.dates[date_idx];
      auto &date_info = asset.date_info[date_idx];

      // Skip if binary already exists
      if (date_info.encoded && Config::SKIP_EXISTING_BINARIES) {
        // Still need to read order count if not set (for load balancing)
        if (!date_info.orders_file.empty() && date_info.order_count == 0) {
          std::vector<L2::Order> orders;
          if (decoder.decode_orders(date_info.orders_file, orders)) {
            date_info.order_count = orders.size();
          }
        }
        progress_handle.update(i + 1, asset.dates.size(), date_str);
        continue;
      }

      // Check archive exists before attempting extraction
      const std::string archive_path = Utils::generate_archive_path(l2_archive_base, date_str);
      if (!std::filesystem::exists(archive_path)) {
        progress_handle.update(i + 1, asset.dates.size(), date_str);
        continue;
      }

      // Encode (blocking on RAR lock if needed)
      bool success = Encoding::extract_and_encode(archive_path, asset.asset_code, date_str, temp_dir, encoder, date_info);

      if (success) {
        date_info.encoded = 1;

        if (Config::CLEANUP_AFTER_PROCESSING) {
          std::filesystem::remove_all(date_info.temp_dir);
        }
      }

      progress_handle.update(i + 1, asset.dates.size(), date_str);
    }
  }
}

// ============================================================================
// PHASE 2: ANALYSIS PROCESSOR (DATE-FIRST TRAVERSAL)
// ============================================================================

// Worker function: date-first processing for assigned assets
void analysis_worker(const SharedState &state,
                     int worker_id,
                     GlobalFeatureStore *feature_store,
                     misc::ProgressHandle progress_handle) {

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
  std::vector<std::unordered_set<std::string>> date_sets;

  for (size_t asset_id : my_asset_ids) {
    const auto &asset = state.assets[asset_id];
    lobs.push_back(std::make_unique<LimitOrderBook>(
        L2::DEFAULT_ENCODER_ORDER_SIZE, asset.exchange_type, feature_store, asset.asset_id));
    decoders.push_back(std::make_unique<L2::BinaryDecoder_L2>(
        L2::DEFAULT_ENCODER_SNAPSHOT_SIZE, L2::DEFAULT_ENCODER_ORDER_SIZE));
    date_sets.emplace_back(asset.dates.begin(), asset.dates.end());
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

      if (date_sets[i].find(date_str) == date_sets[i].end()) {
        continue;
      }

      // Find date index in asset's dates
      auto it = std::find(asset.dates.begin(), asset.dates.end(), date_str);
      size_t asset_date_idx = it - asset.dates.begin();
      const auto &date_info = asset.date_info[asset_date_idx];

      feature_store->set_current_date(asset.asset_id, date_str);

      if (date_info.has_binaries()) {
        cumulative_orders += Analysis::process_binary_files(date_info, *decoders[i], *lobs[i]);
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

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main() {
  try {
    std::cout << "=== L2 Data Processor (CSV Mode) ===" << "\n";

    // Load configuration
    const std::string config_file = Config::DEFAULT_CONFIG_FILE;
    const std::string stock_info_file = Config::DEFAULT_STOCK_INFO_FILE;
    const std::string l2_archive_base = Config::DEFAULT_L2_ARCHIVE_BASE;
    const std::string temp_dir = Config::DEFAULT_TEMP_DIR;

    const JsonConfig::AppConfig app_config = JsonConfig::ParseAppConfig(config_file);
    auto stock_info_map = JsonConfig::ParseStockInfo(stock_info_file);

    // Adjust stock dates based on config
    const std::chrono::year_month config_start{app_config.start_date.year(), app_config.start_date.month()};
    const std::chrono::year_month config_end{app_config.end_date.year(), app_config.end_date.month()};
    for (auto &[code, info] : stock_info_map) {
      if (info.start_date < config_start) {
        info.start_date = config_start;
      }
      if (!info.is_delisted) {
        info.end_date = config_end;
      }
    }

    // Print summary
    std::cout << "Configuration:" << "\n";
    std::cout << "  Archive: " << Config::ARCHIVE_TOOL << " (" << Config::ARCHIVE_EXTENSION << ")\n";
    std::cout << "  L2 base: " << l2_archive_base << "\n";
    std::cout << "  Temp dir: " << temp_dir << "\n";
    std::cout << "  Period: " << JsonConfig::FormatYearMonthDay(app_config.start_date)
              << " → " << JsonConfig::FormatYearMonthDay(app_config.end_date) << "\n";
    std::cout << "  Assets: " << stock_info_map.size() << "\n";
    std::cout << "  Skip existing: " << (Config::SKIP_EXISTING_BINARIES ? "Yes" : "No") << "\n";
    std::cout << "  Auto cleanup: " << (Config::CLEANUP_AFTER_PROCESSING ? "Yes" : "No") << "\n\n";

    std::filesystem::create_directories(temp_dir);
    Logger::init(temp_dir);

    const unsigned int num_threads = misc::Affinity::core_count();
    const unsigned int num_workers = std::min(num_threads, static_cast<unsigned int>(stock_info_map.size()));

    std::cout << "Threads: " << num_threads;
    if (misc::Affinity::supported()) {
      std::cout << " (CPU affinity enabled)";
    }
    std::cout << "\n";
    std::cout << "Workers: " << num_workers << " (processing " << stock_info_map.size() << " assets)\n\n";

    // ========================================================================
    // PHASE 1: ENCODING (can be out-of-order, uses RAR locks)
    // ========================================================================
    std::cout << "=== Phase 1: Encoding ===" << "\n";

    // Build shared state
    SharedState state;
    for (const auto &[asset_code, stock_info] : stock_info_map) {
      const auto effective_start = std::max(std::chrono::year_month_day{stock_info.start_date / std::chrono::day{1}}, app_config.start_date);
      const auto effective_end = std::min(std::chrono::year_month_day{stock_info.end_date / std::chrono::last}, app_config.end_date);

      const auto dates = Utils::generate_date_range(JsonConfig::FormatYearMonthDay(effective_start), JsonConfig::FormatYearMonthDay(effective_end), l2_archive_base);

      if (!dates.empty()) {
        state.assets.emplace_back(state.assets.size(), asset_code, stock_info.name, dates);
      }
    }

    // Initialize paths and dates
    state.init_paths(temp_dir);
    state.finalize_dates();
    state.scan_all_existing_binaries(); // For resume support

    std::cout << "Date range: " << state.all_dates.front() << " → " << state.all_dates.back()
              << " (" << state.all_dates.size() << " trading days)\n";

    // Build asset ID queue for work distribution
    std::vector<size_t> asset_id_queue;
    for (size_t i = 0; i < state.assets.size(); ++i) {
      asset_id_queue.push_back(i);
    }

    std::mutex queue_mutex;
    auto encoding_progress = std::make_shared<misc::ParallelProgress>(num_workers);
    std::vector<std::future<void>> encoding_workers;

    for (unsigned int i = 0; i < num_workers; ++i) {
      encoding_workers.push_back(
          std::async(std::launch::async, encoding_worker, std::ref(state), std::ref(asset_id_queue), std::ref(queue_mutex), std::cref(l2_archive_base), std::cref(temp_dir), i, encoding_progress->acquire_slot("")));
    }

    for (auto &worker : encoding_workers) {
      worker.wait();
    }
    encoding_progress->stop();

    std::cout << "Encoding complete: " << state.assets.size() << " assets ("
              << state.total_encoded_dates() << " date-asset pairs)\n\n";

    // ========================================================================
    // PHASE 2: ANALYSIS
    // ========================================================================
    std::cout << "=== Phase 2: Analysis ===" << "\n";

    // Initialize global feature store
    GlobalFeatureStore feature_store(state.assets.size(), state.all_dates.size());

    // Load balancing: sort assets by order count (already collected during encoding!)
    std::vector<std::pair<size_t, size_t>> asset_workloads; // (asset_id, order_count)
    asset_workloads.reserve(state.assets.size());

    for (size_t i = 0; i < state.assets.size(); ++i) {
      asset_workloads.push_back({i, state.assets[i].get_total_order_count()});
    }

    std::sort(asset_workloads.begin(), asset_workloads.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    // Greedy assignment: each asset goes to worker with minimum current load
    std::vector<size_t> worker_loads(num_workers, 0);

    for (const auto &[asset_id, order_count] : asset_workloads) {
      size_t min_worker = std::min_element(worker_loads.begin(), worker_loads.end()) - worker_loads.begin();
      state.assets[asset_id].assigned_worker_id = min_worker;
      worker_loads[min_worker] += order_count;
    }

    // Launch workers with date-first processing
    auto analysis_progress = std::make_shared<misc::ParallelProgress>(num_workers);
    std::vector<std::future<void>> analysis_workers;

    for (unsigned int i = 0; i < num_workers; ++i) {
      analysis_workers.push_back(std::async(std::launch::async,
                                            analysis_worker,
                                            std::cref(state),
                                            static_cast<int>(i),
                                            &feature_store,
                                            analysis_progress->acquire_slot("")));
    }

    for (auto &worker : analysis_workers) {
      worker.wait();
    }
    analysis_progress->stop();

    // Print feature storage summary
    std::cout << "\n";
    std::cout << "Feature Storage Summary:\n";
    std::cout << "  Total assets: " << feature_store.get_num_assets() << "\n";
    std::cout << "  Total dates: " << feature_store.get_num_dates() << "\n";
    std::cout << "\n";

    Logger::close();

    if (Config::CLEANUP_AFTER_PROCESSING) {
      if (std::filesystem::exists(temp_dir)) {
        std::filesystem::remove_all(temp_dir);
      }
    }

    std::cout << "\n=== Processing Complete ===" << "\n";
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    std::cerr << "Check: config files, " << Config::ARCHIVE_TOOL << " availability, archive path\n";
    std::cerr << "Format: " << Config::DEFAULT_L2_ARCHIVE_BASE << "/YYYY/YYYYMM/YYYYMMDD" << Config::ARCHIVE_EXTENSION << "\n";
    return 1;
  }
}
