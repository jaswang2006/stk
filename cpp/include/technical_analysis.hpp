#pragma once

#include "define/CBuffer.hpp"
#include "define/Dtype.hpp"
#include "math/LimitOrderBook.hpp"
#include <cstdint>
#include <string>
#include <vector>

class TechnicalAnalysis {
public:
  TechnicalAnalysis(size_t capacity);
  ~TechnicalAnalysis();

  // Main interface - processes single snapshot with richer info
  void ProcessSingleSnapshot(const Table::Snapshot_Record &snapshot);

  // Export functionality
  void DumpSnapshotCSV(const std::string &asset_code, const std::string &output_dir, size_t last_n = 0) const;
  void DumpBarCSV(const std::string &asset_code, const std::string &output_dir, size_t last_n = 0) const;

  // Access to internal data for size reporting
  size_t GetSnapshotCount() const { return continuous_snapshots_.size(); }
  size_t GetBarCount() const { return minute_bars_.size(); }

private:
  // Core unified processing function
  inline void ProcessSnapshotInternal(const Table::Snapshot_Record &snapshot);

  // Analysis functions
  inline void AnalyzeSnapshot(const Table::Snapshot_Record &snapshot);
  inline void AnalyzeMinuteBar(const Table::Bar_1m_Record &bar);

  // Bar management
  inline bool IsNewMinuteBar(const Table::Snapshot_Record &snapshot);
  inline void FinalizeCurrentBar();
  inline void StartNewBar(const Table::Snapshot_Record &snapshot);
  inline void UpdateCurrentBar(const Table::Snapshot_Record &snapshot);

  // Helper functions
  inline void GetGapSnapshot(uint32_t timestamp);

  // State tracking - grouped for cache efficiency
  bool has_previous_snapshot_ = false;
  bool has_current_bar_ = false;
  uint32_t last_processed_time_ = 0;
  Table::Snapshot_Record last_snapshot_;
  Table::Snapshot_Record gap_snapshot_;
  Table::Bar_1m_Record current_bar_;

  // Data storage
  std::vector<Table::Snapshot_Record> continuous_snapshots_;
  std::vector<Table::Bar_1m_Record> minute_bars_;

  // Check new session start
  uint8_t last_market_hour_ = 255;
  uint8_t last_market_minute_ = 255;
  uint8_t market_state_ = 0; // state: 0: market-close, 1: pre-market, 2: market, 3: post-market
  inline bool UpdateMarketState(const Table::Snapshot_Record &snapshot);

  // intermediate data for feature computation
  CBuffer<uint16_t, BLen> snapshot_delta_t_;
  CBuffer<float, BLen> snapshot_prices_;
  CBuffer<float, BLen> snapshot_vwaps_;
  CBuffer<float, BLen> snapshot_volumes_;
  CBuffer<uint8_t, BLen> snapshot_directions_;
  CBuffer<float, BLen> snapshot_spreads_;
  CBuffer<float, BLen> snapshot_mid_prices_;

  // features =============================================
  CBuffer<float, BLen> delta_t_;
  CBuffer<float, BLen> norm_spread_;
  CBuffer<std::array<float, 5>, BLen> norm_ofi_ask_;
  CBuffer<std::array<float, 5>, BLen> norm_ofi_bid_;
  // Analysis buffers for minute bar data
  CBuffer<uint32_t, BLen> bar_timestamps_;
  CBuffer<float, BLen> bar_opens_;
  CBuffer<float, BLen> bar_highs_;
  CBuffer<float, BLen> bar_lows_;
  CBuffer<float, BLen> bar_closes_;
  CBuffer<float, BLen> bar_vwaps_;
  CBuffer<float, BLen> bar_volumes_;

  // Limit Order Book for snapshot analysis
  LimitOrderBook<float, uint16_t, uint8_t, BLen> lob;
};

// Configuration constants
inline constexpr float PRICE_EPSILON = 1e-6f;
