#include "codec/binary_decoder_L2.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>

namespace L2 {

bool BinaryDecoder_L2::decode_snapshots_from_binary(const std::string& filepath,
                                            std::vector<Snapshot>& snapshots) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open binary file: " << filepath << std::endl;
        return false;
    }
    
    // Read header: number of snapshots
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (file.fail()) {
        std::cerr << "Failed to read snapshot count from " << filepath << std::endl;
        return false;
    }
    
    snapshots.reserve(count);
    
    // Read all snapshots
    for (size_t i = 0; i < count; i++) {
        Snapshot snapshot;
        file.read(reinterpret_cast<char*>(&snapshot), sizeof(snapshot));
        if (file.fail()) {
            std::cerr << "Failed to read snapshot " << i << " from " << filepath << std::endl;
            return false;
        }
        snapshots.push_back(snapshot);
    }
    
    file.close();
    std::cout << "Successfully decoded " << count << " snapshots from " << filepath << std::endl;
    return true;
}

bool BinaryDecoder_L2::decode_orders_from_binary(const std::string& filepath,
                                         std::vector<Order>& orders) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open binary file: " << filepath << std::endl;
        return false;
    }
    
    // Read header: number of orders
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (file.fail()) {
        std::cerr << "Failed to read order count from " << filepath << std::endl;
        return false;
    }
    
    orders.reserve(count);
    
    // Read all orders
    for (size_t i = 0; i < count; i++) {
        Order order;
        file.read(reinterpret_cast<char*>(&order), sizeof(order));
        if (file.fail()) {
            std::cerr << "Failed to read order " << i << " from " << filepath << std::endl;
            return false;
        }
        orders.push_back(order);
    }
    
    file.close();
    std::cout << "Successfully decoded " << count << " orders from " << filepath << std::endl;
    return true;
}

std::string BinaryDecoder_L2::time_to_string(uint8_t hour, uint8_t minute, uint8_t second, uint8_t millisecond_10ms) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << static_cast<int>(hour) << ":"
        << std::setfill('0') << std::setw(2) << static_cast<int>(minute) << ":"
        << std::setfill('0') << std::setw(2) << static_cast<int>(second);
    
    if (millisecond_10ms > 0) {
        oss << "." << std::setfill('0') << std::setw(2) << static_cast<int>(millisecond_10ms * 10);
    }
    
    return oss.str();
}

double BinaryDecoder_L2::price_to_rmb(uint16_t price_ticks) {
    return static_cast<double>(price_ticks) / 100.0;
}

uint32_t BinaryDecoder_L2::volume_to_shares(uint16_t volume_100shares) {
    return static_cast<uint32_t>(volume_100shares) * 100;
}

const char* BinaryDecoder_L2::order_type_to_string(uint8_t order_type) {
    switch (order_type) {
        case 0: return "MAKER";
        case 1: return "CANCEL";
        case 2: return "CHANGE";
        case 3: return "TAKER";
        default: return "UNKNOWN";
    }
}

const char* BinaryDecoder_L2::order_dir_to_string(uint8_t order_dir) {
    return order_dir == 0 ? "BID" : "ASK";
}

void BinaryDecoder_L2::print_snapshot(const Snapshot& snapshot, size_t index) {
    std::cout << "=== Snapshot " << index << " ===" << std::endl;
    std::cout << "Time: " << time_to_string(snapshot.hour, snapshot.minute, snapshot.second) << std::endl;
    std::cout << "Close: " << std::fixed << std::setprecision(2) << price_to_rmb(snapshot.close) << " RMB" << std::endl;
    std::cout << "High: " << price_to_rmb(snapshot.high) << " RMB" << std::endl;
    std::cout << "Low: " << price_to_rmb(snapshot.low) << " RMB" << std::endl;
    std::cout << "Volume: " << volume_to_shares(snapshot.volume) << " shares" << std::endl;
    std::cout << "Turnover: " << snapshot.turnover << " fen" << std::endl;
    std::cout << "Trade Count: " << static_cast<int>(snapshot.trade_count) << std::endl;
    
    std::cout << "Bid Prices: ";
    for (int i = 0; i < 10; i++) {
        if (snapshot.bid_price_ticks[i] > 0) {
            std::cout << price_to_rmb(snapshot.bid_price_ticks[i]) << " ";
        }
    }
    std::cout << std::endl;
    
    std::cout << "Ask Prices: ";
    for (int i = 0; i < 10; i++) {
        if (snapshot.ask_price_ticks[i] > 0) {
            std::cout << price_to_rmb(snapshot.ask_price_ticks[i]) << " ";
        }
    }
    std::cout << std::endl;
    
    std::cout << "VWAP - Bid: " << price_to_rmb(snapshot.all_bid_vwap) 
              << ", Ask: " << price_to_rmb(snapshot.all_ask_vwap) << std::endl;
    std::cout << "Total Volume - Bid: " << volume_to_shares(snapshot.all_bid_volume)
              << ", Ask: " << volume_to_shares(snapshot.all_ask_volume) << std::endl;
    std::cout << std::endl;
}

void BinaryDecoder_L2::print_order(const Order& order, size_t index) {
    std::cout << "=== Order " << index << " ===" << std::endl;
    std::cout << "Time: " << time_to_string(order.hour, order.minute, order.second, order.millisecond) << std::endl;
    std::cout << "Type: " << order_type_to_string(order.order_type) << std::endl;
    std::cout << "Direction: " << order_dir_to_string(order.order_dir) << std::endl;
    std::cout << "Price: " << std::fixed << std::setprecision(2) << price_to_rmb(order.price) << " RMB" << std::endl;
    std::cout << "Volume: " << volume_to_shares(order.volume) << " shares" << std::endl;
    std::cout << "Bid Order ID: " << order.bid_order_id << std::endl;
    std::cout << "Ask Order ID: " << order.ask_order_id << std::endl;
    std::cout << std::endl;
}

void BinaryDecoder_L2::print_all_snapshots(const std::vector<Snapshot>& snapshots) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "PRINTING ALL " << snapshots.size() << " SNAPSHOTS WITH ARRAY DETAILS" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    for (size_t i = 0; i < snapshots.size(); i++) {
        const auto& snapshot = snapshots[i];
        
        std::cout << "Snapshot[" << i << "]:" << std::endl;
        std::cout << "  Time: " << time_to_string(snapshot.hour, snapshot.minute, snapshot.second) << std::endl;
        std::cout << "  hour=" << static_cast<int>(snapshot.hour) 
                  << ", minute=" << static_cast<int>(snapshot.minute) 
                  << ", second=" << static_cast<int>(snapshot.second) << std::endl;
        
        std::cout << "  Price Info: close=" << price_to_rmb(snapshot.close) 
                  << ", high=" << price_to_rmb(snapshot.high) 
                  << ", low=" << price_to_rmb(snapshot.low) << std::endl;
        
        std::cout << "  Volume Info: volume=" << volume_to_shares(snapshot.volume) 
                  << ", turnover=" << snapshot.turnover 
                  << ", trade_count=" << static_cast<int>(snapshot.trade_count) << std::endl;
        
        std::cout << "  bid_price_ticks[10]: ";
        for (int j = 0; j < 10; j++) {
            std::cout << snapshot.bid_price_ticks[j] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  bid_volumes[10]: ";
        for (int j = 0; j < 10; j++) {
            std::cout << snapshot.bid_volumes[j] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  ask_price_ticks[10]: ";
        for (int j = 0; j < 10; j++) {
            std::cout << snapshot.ask_price_ticks[j] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  ask_volumes[10]: ";
        for (int j = 0; j < 10; j++) {
            std::cout << snapshot.ask_volumes[j] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  direction=" << (snapshot.direction ? "SELL" : "BUY") << std::endl;
        std::cout << "  all_bid_vwap=" << snapshot.all_bid_vwap 
                  << ", all_ask_vwap=" << snapshot.all_ask_vwap << std::endl;
        std::cout << "  all_bid_volume=" << snapshot.all_bid_volume 
                  << ", all_ask_volume=" << snapshot.all_ask_volume << std::endl;
        
        std::cout << std::endl;
    }
}

void BinaryDecoder_L2::print_all_orders(const std::vector<Order>& orders) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "PRINTING ALL " << orders.size() << " ORDERS WITH ARRAY DETAILS" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    for (size_t i = 0; i < orders.size(); i++) {
        const auto& order = orders[i];
        
        std::cout << "Order[" << i << "]:" << std::endl;
        std::cout << "  Time: " << time_to_string(order.hour, order.minute, order.second, order.millisecond) << std::endl;
        std::cout << "  hour=" << static_cast<int>(order.hour) 
                  << ", minute=" << static_cast<int>(order.minute) 
                  << ", second=" << static_cast<int>(order.second) 
                  << ", millisecond=" << static_cast<int>(order.millisecond) << std::endl;
        
        std::cout << "  order_type=" << static_cast<int>(order.order_type) << " (" << order_type_to_string(order.order_type) << ")" << std::endl;
        std::cout << "  order_dir=" << static_cast<int>(order.order_dir) << " (" << order_dir_to_string(order.order_dir) << ")" << std::endl;
        std::cout << "  price=" << order.price << " (" << price_to_rmb(order.price) << " RMB)" << std::endl;
        std::cout << "  volume=" << order.volume << " (" << volume_to_shares(order.volume) << " shares)" << std::endl;
        std::cout << "  bid_order_id=" << order.bid_order_id << std::endl;
        std::cout << "  ask_order_id=" << order.ask_order_id << std::endl;
        
        std::cout << std::endl;
    }
}

} // namespace L2
