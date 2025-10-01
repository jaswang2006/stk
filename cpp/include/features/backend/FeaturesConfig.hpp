#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Event-driven feature level definitions

// Level 0: Base level features (most frequent)
#define LEVEL_0_FIELDS(X)                                    \
  X(last_price, float, "Last traded price")                  \
  X(bid1, float, "Best bid price")                           \
  X(ask1, float, "Best ask price")                           \
  X(volume, uint32_t, "Trade volume")                        \
  X(bid1_size, uint32_t, "Best bid size")                    \
  X(ask1_size, uint32_t, "Best ask size")                    \
  X(spread, float, "Bid-ask spread")                         \
  X(mid_price, float, "Mid price")                           \
  X(is_uptick, bool, "Is uptick indicator")                  \
  X(micro_price, float, "Micro price")                       \
  X(order_imbalance, float, "Order flow imbalance")          \
  X(trade_sign, float, "Trade direction sign")               \
  X(aggressive_buy_vol, uint32_t, "Aggressive buy volume")   \
  X(aggressive_sell_vol, uint32_t, "Aggressive sell volume") \
  X(effective_spread, float, "Effective spread")             \
  X(realized_spread, float, "Realized spread")               \
  X(price_impact, float, "Price impact")                     \
  X(event_id, uint64_t, "Event identifier")

// Level 1: Aggregated features (mid frequency)
#define LEVEL_1_FIELDS(X)                                 \
  X(vwap, double, "Volume weighted average price")        \
  X(twap, double, "Time weighted average price")          \
  X(high, double, "High price")                           \
  X(low, double, "Low price")                             \
  X(open, double, "Open price")                           \
  X(close, double, "Close price")                         \
  X(total_volume, uint64_t, "Total volume")               \
  X(total_trades, uint64_t, "Total number of trades")     \
  X(buy_trades, uint32_t, "Number of buy trades")         \
  X(sell_trades, uint32_t, "Number of sell trades")       \
  X(price_volatility, double, "Price volatility")         \
  X(volume_volatility, double, "Volume volatility")       \
  X(skewness, double, "Price distribution skewness")      \
  X(kurtosis, double, "Price distribution kurtosis")      \
  X(avg_spread, double, "Average spread")                 \
  X(avg_depth, double, "Average depth")                   \
  X(order_flow_imbalance, double, "Order flow imbalance") \
  X(rsi, double, "Relative strength index")               \
  X(momentum_short, double, "Short-term momentum")        \
  X(momentum_long, double, "Long-term momentum")          \
  X(event_id, uint64_t, "Event identifier")

// Level 2: High-level aggregated features (lowest frequency)
#define LEVEL_2_FIELDS(X)                                    \
  X(vwap, double, "Volume weighted average price")           \
  X(high, double, "High price")                              \
  X(low, double, "Low price")                                \
  X(open, double, "Open price")                              \
  X(close, double, "Close price")                            \
  X(total_volume, uint64_t, "Total volume")                  \
  X(total_trades, uint64_t, "Total number of trades")        \
  X(volatility, double, "Long-term volatility")              \
  X(return_rate, double, "Return rate")                      \
  X(volume_profile_shape, double, "Volume profile shape")    \
  X(participation_rate, double, "Market participation rate") \
  X(sector_correlation, double, "Sector correlation")        \
  X(market_beta, double, "Market beta")                      \
  X(relative_strength, double, "Relative strength")          \
  X(funding_rate_impact, double, "Funding rate impact")      \
  X(options_flow_impact, double, "Options flow impact")      \
  X(event_id, uint64_t, "Event identifier")

// All levels definition - USER EXTENDS HERE for L4/5/6/7
#define ALL_LEVELS(X)                      \
  X(L0, 0, LEVEL_0_FIELDS, Level0Features) \
  X(L1, 1, LEVEL_1_FIELDS, Level1Features) \
  X(L2, 2, LEVEL_2_FIELDS, Level2Features)

// =================================================================
// AUTO-GENERATED CODE - ALL EXTENSIONS HANDLED BY MACROS
// =================================================================

// Auto-generate level count and indices
#define COUNT_LEVELS(level_name, level_num, fields, struct_name) +1
constexpr size_t LEVEL_COUNT = 0 ALL_LEVELS(COUNT_LEVELS);

#define GET_LEVEL_INDEX(level_name, level_num, fields, struct_name) \
  constexpr size_t level_name##_INDEX = level_num;
ALL_LEVELS(GET_LEVEL_INDEX)

// =================================================================
// USER MACROS - ONLY WHAT USER NEEDS FOR EXTENSION
// =================================================================

// Basic struct field generation (used by level struct generation)
#define STRUCT_FIELD(name, type, comment) type name;

// Auto-generate level structures - USER ONLY NEEDS TO MODIFY ALL_LEVELS
#define GENERATE_LEVEL_STRUCT(level_name, level_num, fields, struct_name) \
  struct struct_name {                                                    \
    fields(STRUCT_FIELD)                                                  \
  };
ALL_LEVELS(GENERATE_LEVEL_STRUCT)
