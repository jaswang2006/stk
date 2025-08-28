#pragma once

#include "L2_DataType.hpp"
#include <vector>
#include <string>

namespace L2 {

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
    
    // Convert volume from internal format to readable format
    static uint32_t volume_to_shares(uint16_t volume_100shares);

private:
    static const char* order_type_to_string(uint8_t order_type);
    static const char* order_dir_to_string(uint8_t order_dir);
};

} // namespace L2
