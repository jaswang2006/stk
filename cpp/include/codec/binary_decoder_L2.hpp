#pragma once

#include "L2_DataType.hpp"
#include <vector>
#include <string>
#include <cmath>

namespace L2 {

// Inline constexpr functions to calculate column widths from bit widths
// These will be optimized away at compile time
namespace BitWidthFormat {
    // Calculate number of decimal digits needed for a given bit width
    inline constexpr int calc_width(uint8_t bit_width) {
        return bit_width <= 3 ? 1 :  // 1-3 bits: 1 digit
               bit_width <= 6 ? 2 :  // 4-6 bits: 2 digits  
               bit_width <= 10 ? 3 : // 7-10 bits: 3 digits
               bit_width <= 13 ? 4 : // 11-13 bits: 4 digits
               bit_width <= 16 ? 5 : // 14-16 bits: 5 digits
               bit_width <= 32 ? 10 : 15; // 17-32 bits: 10 digits, >32: 15 digits
    }
    
    // Snapshot field widths - using schema indices
    inline constexpr int hour_width()       { return calc_width(Snapshot_Schema[0].bit_width); }   // 5bit -> 2
    inline constexpr int minute_width()     { return calc_width(Snapshot_Schema[1].bit_width); }   // 6bit -> 2
    inline constexpr int second_width()     { return calc_width(Snapshot_Schema[2].bit_width); }   // 6bit -> 2
    inline constexpr int trade_count_width(){ return calc_width(Snapshot_Schema[3].bit_width); }   // 8bit -> 3
    inline constexpr int volume_width()     { return calc_width(Snapshot_Schema[4].bit_width); }   // 16bit -> 5
    inline constexpr int turnover_width()   { return calc_width(Snapshot_Schema[5].bit_width); }   // 32bit -> 10
    inline constexpr int price_width()      { return calc_width(Snapshot_Schema[6].bit_width); }   // 14bit -> 5 (high/low/close)
    inline constexpr int direction_width()  { return calc_width(Snapshot_Schema[13].bit_width); }  // 1bit -> 1
    inline constexpr int vwap_width()       { return calc_width(Snapshot_Schema[14].bit_width); }  // 14bit -> 5
    inline constexpr int total_volume_width() { return calc_width(Snapshot_Schema[16].bit_width); } // 14bit -> 5
    
    // Order field widths - using order schema indices (start from index 17)
    inline constexpr int order_type_width() { return calc_width(Snapshot_Schema[17].bit_width); }  // 2bit -> 1
    inline constexpr int order_dir_width()  { return calc_width(Snapshot_Schema[18].bit_width); }  // 1bit -> 1
    inline constexpr int order_price_width(){ return calc_width(Snapshot_Schema[19].bit_width); }  // 14bit -> 5
    inline constexpr int order_volume_width(){ return calc_width(Snapshot_Schema[20].bit_width); } // 16bit -> 5
    inline constexpr int order_id_width()   { return calc_width(Snapshot_Schema[21].bit_width); }  // 32bit -> 10
    inline constexpr int millisecond_width(){ return 3; } // 7bit for millisecond (in 10ms units)
}

class BinaryDecoder_L2 {
public:
    // Decode snapshots from binary file
    static bool decode_snapshots_from_binary(const std::string& filepath,
                                            std::vector<Snapshot>& snapshots);
    
    // Decode orders from binary file
    static bool decode_orders_from_binary(const std::string& filepath,
                                         std::vector<Order>& orders);
    
    // Print snapshot in human-readable format
    static void print_snapshot(const Snapshot& snapshot, size_t index = 0);
    
    // Print order in human-readable format
    static void print_order(const Order& order, size_t index = 0);
    
    // Print all snapshots with array details
    static void print_all_snapshots(const std::vector<Snapshot>& snapshots);
    
    // Print all orders with array details
    static void print_all_orders(const std::vector<Order>& orders);
    
    // Convert time components back to readable format
    static std::string time_to_string(uint8_t hour, uint8_t minute, uint8_t second, uint8_t millisecond_10ms = 0);
    
    // Convert price from internal format to readable format
    static double price_to_rmb(uint16_t price_ticks);
    
    // Convert VWAP price from internal format to readable format (0.001 RMB units)
    static double vwap_to_rmb(uint16_t vwap_ticks);
    
    // Convert volume from internal format to readable format
    static uint32_t volume_to_shares(uint16_t volume_100shares);

private:
    static const char* order_type_to_string(uint8_t order_type);
    static const char* order_dir_to_string(uint8_t order_dir);
};

} // namespace L2
