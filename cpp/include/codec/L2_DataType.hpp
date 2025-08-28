#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace L2 {

// modern compression algo maynot benefit from delta encoding
inline constexpr bool ENABLE_DELTA_ENCODING = false; // use Zstd for high compress ratio and fast decompress speed

// | Compressor name     | Ratio | Compression | Decompress |
// |---------------------|-------|-------------|------------|
// | zstd 1.5.7 -1       | 2.896 | 510 MB/s    | 1550 MB/s  |
// | brotli 1.1.0 -1     | 2.883 | 290 MB/s    | 425 MB/s   |
// | zlib 1.3.1 -1       | 2.743 | 105 MB/s    | 390 MB/s   |
// | zstd 1.5.7 --fast=1 | 2.439 | 545 MB/s    | 1850 MB/s  |
// | quicklz 1.5.0 -1    | 2.238 | 520 MB/s    | 750 MB/s   |
// | zstd 1.5.7 --fast=4 | 2.146 | 665 MB/s    | 2050 MB/s  |
// | lzo1x 2.10 -1       | 2.106 | 650 MB/s    | 780 MB/s   |
// | lz4 1.10.0          | 2.101 | 675 MB/s    | 3850 MB/s  |
// | snappy 1.2.1        | 2.089 | 520 MB/s    | 1500 MB/s  |
// | lzf 3.6 -1          | 2.077 | 410 MB/s    | 820 MB/s   |

enum class DataType { INT,
                      FLOAT,
                      DOUBLE,
                      BOOL };

struct ColumnMeta {
  std::string_view column_name; // 列名
  DataType data_type;           // 运算数据类型
  bool is_signed;               // 原始数据(或其增量数据)是否有符号
  uint8_t bit_width;            // 实际存储 bit 宽度
  bool use_delta;               // 是否存 delta 编码
};

// clang-format off
constexpr ColumnMeta Snapshot_Schema[] = {
    // snapshot
    {"hour",               DataType::INT,   true, 5,    true  },// "取值范围 0-23，5bit 足够，取值连续, 使用delta编码"},
    {"minute",             DataType::INT,   true, 6,    true  },// "取值范围 0-59，6bit 足够，取值连续, 使用delta编码"},
    {"second",             DataType::INT,   true, 6,    true  },// "取值范围 0-59，6bit 足够，取值连续, 使用delta编码"},
    {"trade_count",        DataType::INT,   false, 8,   false },// "波动较大, 多数时候为0或小值, 直接存储"},
    {"volume",             DataType::INT,   false, 16,  false },// "波动较大，但也有大量0, 直接存储"},
    {"turnover",           DataType::INT,   false, 32,  false },// "波动较大，但也有大量0, 直接存储"},
    {"high",               DataType::INT,   true,  14,  true  },// "价格连续(0.01 RMB units)，使用delta编码"},
    {"low",                DataType::INT,   true,  14,  true  },// "价格连续(0.01 RMB units)，使用delta编码"},
    {"close",              DataType::INT,   true,  14,  true  },// "价格连续(0.01 RMB units)，使用delta编码"},
    {"bid_price_ticks[10]",DataType::INT,   true,  14,  true  },// "订单价长时间静态(0.01 RMB units)，局部跳变，使用delta编码"},
    {"bid_volumes[10]",    DataType::INT,   false, 14,  false },// "订单量长时间静态，局部跳变，直接存储"},
    {"ask_price_ticks[10]",DataType::INT,   true,  14,  true  },// "订单价长时间静态(0.01 RMB units)，局部跳变，使用delta编码"},
    {"ask_volumes[10]",    DataType::INT,   false, 14,  false },// "订单量长时间静态，局部跳变，直接存储"},
    {"direction",          DataType::BOOL,  false, 1,   false },// "仅买/卖两种值，直接存储"},
    {"all_bid_vwap",       DataType::INT,   true,  15,  true  },// "VWAP价格连续(0.001 RMB units)，使用delta编码"},
    {"all_ask_vwap",       DataType::INT,   true,  15,  true  },// "VWAP价格连续(0.001 RMB units)，使用delta编码"},
    {"all_bid_volume",     DataType::INT,   true, 22,   true  },// "总量变化平滑，使用delta编码"},
    {"all_ask_volume",     DataType::INT,   true, 22,   true  },// "总量变化平滑，使用delta编码"},

    // order
    {"millisecon",        DataType::INT,   true, 7,     true  },// "取值范围 0-127，7bit 足够，取值连续, 使用delta编码"},
    {"order_type",         DataType::INT,   false, 2,   false },// "仅增删改成交四种值，直接存储"},
    {"order_dir",          DataType::BOOL,  false, 1,   false },// "仅bid ask 两种值，直接存储"},
    {"price",              DataType::INT,   true,  14,  true  },// "价格连续(0.01 RMB units)，使用delta编码"},
    {"volume",             DataType::INT,   false, 16,  false },// "大部分绝对值小，直接存储"},
    {"bid_order_id",       DataType::INT,   true, 32,   true  },// "订单id大部分递增，局部跳变，使用delta编码"},
    {"ask_order_id",       DataType::INT,   true, 32,   true  },// "订单id大部分递增，局部跳变，使用delta编码"},
  };
// clang-format on

struct Snapshot {
  uint8_t hour;                 // 5bit
  uint8_t minute;               // 6bit
  uint8_t second;               // 6bit
  uint8_t trade_count;          // 8bit
  uint16_t volume;              // 16bit - units of 100 shares
  uint32_t turnover;            // 32bit - RMB
  uint16_t high;                // 14bit - price in 0.01 RMB units
  uint16_t low;                 // 14bit - price in 0.01 RMB units
  uint16_t close;               // 14bit - price in 0.01 RMB units
  uint16_t bid_price_ticks[10]; // 14bits * 10 - prices in 0.01 RMB units
  uint16_t bid_volumes[10];     // 14bits * 10 - units of 100 shares
  uint16_t ask_price_ticks[10]; // 14bits * 10 - prices in 0.01 RMB units
  uint16_t ask_volumes[10];     // 14bits * 10 - units of 100 shares
  bool direction;               // 1bit - 0: buy, 1: sell (vwap_last > vwap_now)
  uint16_t all_bid_vwap;        // 15bit - vwap in 0.001 RMB units of all bid orders
  uint16_t all_ask_vwap;        // 15bit - vwap in 0.001 RMB units of all ask orders
  uint32_t all_bid_volume;      // 22bit - volume of all bid orders in 100 shares
  uint32_t all_ask_volume;      // 22bit - volume of all ask orders in 100 shares
};

// 合并逐笔委托(增删改成交)
struct Order {
  uint8_t hour;        // 5bit
  uint8_t minute;      // 6bit
  uint8_t second;      // 6bit
  uint8_t millisecond; // 7bit (in 10ms)

  uint8_t order_type; // 2bit - 0:maker(order) 1:cancel 2:change 3:taker(trade)
  uint8_t order_dir;  // 1bit - 0:bid 1:ask
  uint16_t price;     // 14bit - price in 0.01 RMB units
  uint16_t volume;    // 16bit - units of 100 shares

  uint32_t bid_order_id; // 32bit
  uint32_t ask_order_id; // 32bit
  // (order_type, order_dir)== |(0,0)        |(0,1)         |(1,0)         |(1,1)          |(2,0) |(2,1) |(3,0)         |(3,1)
  // bid_order_id:             |buy_maker_id |0             |buy_cancel_id |0              |0     |0     |buy_taker_id  |buy_maker_id
  // ask_order_id:             |0            |sell_maker_id |0             |sell_cancel_id |0     |0     |sell_maker_id |sell_taker_id
};

// Compile-time schema field lookup and bounds calculation
namespace SchemaUtils {
// Helper to find column index by name in schema
constexpr size_t find_column_index(const ColumnMeta *schema, size_t schema_size, std::string_view column_name) {
  for (size_t i = 0; i < schema_size; ++i) {
    if (schema[i].column_name == column_name) {
      return i;
    }
  }
  return schema_size; // Return invalid index if not found
}

// Get bitwidth for a column from schema
constexpr uint8_t get_column_bitwidth(const ColumnMeta *schema, size_t schema_size, std::string_view column_name) {
  size_t index = find_column_index(schema, schema_size, column_name);
  return (index < schema_size) ? schema[index].bit_width : 0;
}

// Calculate max value from bitwidth
constexpr uint64_t bitwidth_to_max(uint8_t bitwidth) {
  return bitwidth > 0 ? ((1ull << bitwidth) - 1) : 0;
}
} // namespace SchemaUtils

// Compile-time upper bound calculations based on schema definitions
namespace BitwidthBounds {
constexpr size_t SCHEMA_SIZE = sizeof(Snapshot_Schema) / sizeof(Snapshot_Schema[0]);

// Snapshot field upper bounds extracted from schema
constexpr uint32_t HOUR_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "hour"));
constexpr uint32_t MINUTE_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "minute"));
constexpr uint32_t SECOND_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "second"));
constexpr uint32_t TRADE_COUNT_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "trade_count"));
constexpr uint32_t VOLUME_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "volume"));
constexpr uint64_t TURNOVER_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "turnover"));
constexpr uint32_t PRICE_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "high"));
constexpr uint32_t ORDERBOOK_VOLUME_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "bid_volumes[10]"));
constexpr uint32_t VWAP_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "all_bid_vwap"));
constexpr uint32_t TOTAL_VOLUME_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "all_bid_volume"));

// Order field upper bounds extracted from schema
constexpr uint32_t MILLISECOND_BOUND = 127; // 7 bits for millisecond in 10ms units (not in schema)
constexpr uint32_t ORDER_TYPE_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "order_type"));
constexpr uint32_t ORDER_DIR_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "order_dir"));
constexpr uint64_t ORDER_ID_BOUND = SchemaUtils::bitwidth_to_max(
    SchemaUtils::get_column_bitwidth(Snapshot_Schema, SCHEMA_SIZE, "bid_order_id"));

// Helper functions for safe casting with bounds checking
template <typename T>
constexpr T clamp_to_bound(uint64_t value, T bound_val) {
  return static_cast<T>(value > bound_val ? bound_val : value);
}
} // namespace BitwidthBounds

} // namespace L2
