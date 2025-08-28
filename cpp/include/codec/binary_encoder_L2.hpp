#pragma once

#include "L2_DataType.hpp"
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>

namespace L2 {

// Intermediate CSV data structures for parsing
struct CSVSnapshot {
    std::string stock_code;
    std::string exchange_code;
    uint32_t date;
    uint32_t time;
    uint32_t price;           // in fen (1/100 RMB)
    uint32_t volume;          // in 100 shares
    uint64_t turnover;        // in fen
    uint32_t trade_count;
    
    uint32_t high;
    uint32_t low;
    uint32_t open;
    uint32_t prev_close;
    
    // bid/ask prices and volumes (10 levels each)
    uint32_t bid_prices[10];
    uint32_t bid_volumes[10];
    uint32_t ask_prices[10];
    uint32_t ask_volumes[10];
    
    uint32_t weighted_avg_ask_price;
    uint32_t weighted_avg_bid_price;
    uint32_t total_ask_volume;
    uint32_t total_bid_volume;
};

struct CSVOrder {
    std::string stock_code;
    std::string exchange_code;
    uint32_t date;
    uint32_t time;
    uint64_t order_id;
    uint64_t exchange_order_id;
    char order_type;     // A:add, D:delete for SSE; 0 for SZSE
    char order_side;     // B:bid, S:ask
    uint32_t price;      // in fen
    uint32_t volume;     // in 100 shares
};

struct CSVTrade {
    std::string stock_code;
    std::string exchange_code;
    uint32_t date;
    uint32_t time;
    uint64_t trade_id;
    char trade_code;     // 0:trade, C:cancel for SZSE; empty for SSE
    char dummy_code;     // not used
    char bs_flag;        // B:buy, S:sell, empty:cancel
    uint32_t price;      // in fen
    uint32_t volume;     // in 100 shares
    uint64_t ask_order_id;
    uint64_t bid_order_id;
};

class BinaryEncoder_L2 {
public:
    // CSV parsing functions
    static std::vector<std::string> split_csv_line(const std::string& line);
    static uint32_t parse_time_to_ms(uint32_t time_int);
    static uint32_t parse_price_to_fen(const std::string& price_str);
    static uint32_t parse_volume_to_100shares(const std::string& volume_str);
    static uint64_t parse_turnover_to_fen(const std::string& turnover_str);
    
    static bool parse_snapshot_csv(const std::string& filepath, std::vector<CSVSnapshot>& snapshots);
    static bool parse_order_csv(const std::string& filepath, std::vector<CSVOrder>& orders);
    static bool parse_trade_csv(const std::string& filepath, std::vector<CSVTrade>& trades);

    // CSV to L2 conversion functions
    static Snapshot csv_to_snapshot(const CSVSnapshot& csv_snap);
    static Order csv_to_order(const CSVOrder& csv_order);
    static Order csv_to_trade_order(const CSVTrade& csv_trade);
    
    // Binary encoding functions
    static bool encode_snapshots_to_binary(const std::vector<Snapshot>& snapshots, 
                                          const std::string& filepath);
    static bool encode_orders_to_binary(const std::vector<Order>& orders,
                                       const std::string& filepath);
    
    // High-level processing function
    static bool process_stock_data(const std::string& stock_dir,
                                  const std::string& output_dir,
                                  const std::string& stock_code);

private:
    static uint8_t time_to_hour(uint32_t time_ms);
    static uint8_t time_to_minute(uint32_t time_ms);
    static uint8_t time_to_second(uint32_t time_ms);
    static uint8_t time_to_millisecond_10ms(uint32_t time_ms);
    
    static uint8_t determine_order_type(char csv_order_type, char csv_trade_code, bool is_trade);
    static bool determine_order_direction(char side_flag);
};

} // namespace L2
