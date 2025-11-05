#pragma once

// ============================================================================
// FEATURE DEFINITIONS - All Levels Schema
// ============================================================================
// This file contains ONLY feature field definitions for all levels
// No computation logic, no storage implementation - pure schema

// ============================================================================
// LEVEL 0: Tick-level Features (Highest Frequency)
// ============================================================================

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
// LEVEL 1: Minute-level Features (Medium Frequency)
// ============================================================================

#define LEVEL_1_FIELDS(X)                                                    \
  X(timestamp,      "Minute start timestamp"                             )  \
  X(vwap,           "Volume-weighted average price"                      )  \
  X(high,           "Highest mid price"                                  )  \
  X(low,            "Lowest mid price"                                   )  \
  X(open,           "First mid price"                                    )  \
  X(close,          "Last mid price"                                     )  \
  X(tick_count,     "Number of ticks"                                    )  \
  X(mean_spread,    "Mean spread"                                        )  \
  X(mean_tobi,      "Mean top-of-book imbalance"                         )  \
  X(volatility,     "Price volatility (std)"                             )

// ============================================================================
// LEVEL 2: Hour-level Features (Low Frequency) - Placeholder
// ============================================================================

#define LEVEL_2_FIELDS(X)                                                    \
  X(timestamp,      "Hour start timestamp"                               )  \
  X(vwap,           "Volume-weighted average price"                      )  \
  X(high,           "Highest price"                                      )  \
  X(low,            "Lowest price"                                       )

// ============================================================================
// ALL LEVELS REGISTRY
// ============================================================================
// Format: X(level_name, level_index, fields_macro)

#define ALL_LEVELS(X)                      \
  X(L0, 0, LEVEL_0_FIELDS)                 \
  X(L1, 1, LEVEL_1_FIELDS)                 \
  X(L2, 2, LEVEL_2_FIELDS)

