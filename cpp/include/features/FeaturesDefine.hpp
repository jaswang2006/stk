#pragma once

// ============================================================================
// FEATURE DEFINITIONS - All Levels Schema
// ============================================================================
// This file contains ONLY feature field definitions for all levels
// No computation logic, no storage implementation - pure schema

// ============================================================================
// LEVEL 0: Tick-level Features (Highest Frequency)
// ============================================================================

#include <cstddef>
#include <cstdint>

#define LEVEL_0_FIELDS(X)                                                    \
  X(timestamp,      "Event timestamp in nanoseconds"                      )  \
  X(mid_price,      "Mid price (bid+ask)/2"                              )  \
  X(spread,         "Bid-ask spread"                                     )  \
  X(spread_z,       "Spread z-score"                                     )  \
  X(tobi,           "Top-of-book imbalance"                              )  \
  X(tobi_z,         "Top-of-book imbalance z-score"                      )  \
  X(micro_price,    "Volume-weighted micro price"                        )  \
  X(mpg,            "Micro-price gap (micro - mid)"                      )  \
  X(mpg_z,          "Micro-price gap z-score"                            )

// ============================================================================
// LEVEL 1: Minute-level Features (1 minute intervals) - Candle/OHLC
// ============================================================================

#define LEVEL_1_FIELDS(X)                                                    \
  X(timestamp,      "Minute start timestamp"                             )  \
  X(open,           "Open price (first mid price)"                       )  \
  X(high,           "High price (max mid price)"                         )  \
  X(low,            "Low price (min mid price)"                          )  \
  X(close,          "Close price (last mid price)"                       )  \
  X(vwap,           "Volume-weighted average price"                      )  \
  X(volume,         "Total volume (tick count proxy)"                    )

// ============================================================================
// LEVEL 2: Hour-level Features (1 hour intervals) - Support/Resistance
// ============================================================================

#define LEVEL_2_FIELDS(X)                                                    \
  X(timestamp,      "Hour start timestamp"                               )  \
  X(support_level,  "Support level (weighted low prices)"                )  \
  X(resistance_level,"Resistance level (weighted high prices)"           )  \
  X(pivot_point,    "Pivot point ((high + low + close) / 3)"            )  \
  X(price_range,    "Price range (high - low)"                           )  \
  X(dominant_side,  "Dominant side (1=buy, -1=sell, 0=neutral)"          )

// ============================================================================
// ALL LEVELS REGISTRY
// ============================================================================
// Format: X(level_name, level_index, fields_macro)

#define ALL_LEVELS(X)                      \
  X(L0, 0, LEVEL_0_FIELDS)                 \
  X(L1, 1, LEVEL_1_FIELDS)                 \
  X(L2, 2, LEVEL_2_FIELDS)

// ============================================================================
// TIME GRANULARITY CONFIGURATION
// ============================================================================
// Each level's time interval in milliseconds and daily capacity

constexpr size_t TRADE_HOURS_PER_DAY = 4;
constexpr size_t MS_PRE_DAY = TRADE_HOURS_PER_DAY * 3600000;

// Level 0: Tick-level (100ms per time index)
constexpr size_t L0_TIME_INTERVAL_MS = 1000;
constexpr size_t L0_MAX_TIME_INDEX_PER_DAY = MS_PRE_DAY / L0_TIME_INTERVAL_MS;

// Level 1: Minute-level (1min per time index)
constexpr size_t L1_TIME_INTERVAL_MS = 60'000;
constexpr size_t L1_MAX_TIME_INDEX_PER_DAY = MS_PRE_DAY / L1_TIME_INTERVAL_MS;

// Level 2: Hour-level (1hour per time index)
constexpr size_t L2_TIME_INTERVAL_MS = 3'600'000;
constexpr size_t L2_MAX_TIME_INDEX_PER_DAY = MS_PRE_DAY / L2_TIME_INTERVAL_MS;

// ============================================================================
// TIME INDEX CONVERSION (hour, minute, second, millisecond → time_idx)
// ============================================================================

// Convert time components to milliseconds since start of day
inline constexpr size_t time_to_ms(uint8_t hour, uint8_t minute, uint8_t second, uint8_t millisecond) {
  return hour * 3600000ULL + minute * 60000ULL + second * 1000ULL + millisecond * 10ULL;
}

// Convert time components to Level 0 time index (100ms granularity)
inline constexpr size_t time_to_L0_index(uint8_t hour, uint8_t minute, uint8_t second, uint8_t millisecond) {
  return time_to_ms(hour, minute, second, millisecond) / L0_TIME_INTERVAL_MS;
}

// Convert time components to Level 1 time index (30s granularity)
inline constexpr size_t time_to_L1_index(uint8_t hour, uint8_t minute, uint8_t second, uint8_t millisecond) {
  return time_to_ms(hour, minute, second, millisecond) / L1_TIME_INTERVAL_MS;
}

// Convert time components to Level 2 time index (15min granularity)
inline constexpr size_t time_to_L2_index(uint8_t hour, uint8_t minute, uint8_t second, uint8_t millisecond) {
  return time_to_ms(hour, minute, second, millisecond) / L2_TIME_INTERVAL_MS;
}

