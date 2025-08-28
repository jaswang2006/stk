#include "codec/binary_encoder_L2.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace L2 {

// CSV parsing functions
std::vector<std::string> BinaryEncoder_L2::split_csv_line(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    
    return result;
}

uint32_t BinaryEncoder_L2::parse_time_to_ms(uint32_t time_int) {
    // time format: HHMMSSMS (8 digits)
    uint32_t ms = time_int % 1000;
    time_int /= 1000;
    uint32_t second = time_int % 100;
    time_int /= 100;
    uint32_t minute = time_int % 100;
    time_int /= 100;
    uint32_t hour = time_int;
    
    return hour * 3600000 + minute * 60000 + second * 1000 + ms;
}

uint32_t BinaryEncoder_L2::parse_price_to_fen(const std::string& price_str) {
    if (price_str.empty() || price_str == " " || price_str == "\0") {
        return 0;
    }
    return static_cast<uint32_t>(std::stod(price_str));
}

uint32_t BinaryEncoder_L2::parse_volume_to_100shares(const std::string& volume_str) {
    if (volume_str.empty() || volume_str == " " || volume_str == "\0") {
        return 0;
    }
    return static_cast<uint32_t>(std::stoll(volume_str));
}

uint64_t BinaryEncoder_L2::parse_turnover_to_fen(const std::string& turnover_str) {
    if (turnover_str.empty() || turnover_str == " " || turnover_str == "\0") {
        return 0;
    }
    return static_cast<uint64_t>(std::stod(turnover_str));
}

bool BinaryEncoder_L2::parse_snapshot_csv(const std::string& filepath, std::vector<CSVSnapshot>& snapshots) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    std::string line;
    // Skip header line
    if (!std::getline(file, line)) {
        std::cerr << "Empty file: " << filepath << std::endl;
        return false;
    }
    
    while (std::getline(file, line)) {
        auto fields = split_csv_line(line);
        if (fields.size() < 65) { // Expected minimum number of fields
            continue;
        }
        
        CSVSnapshot snapshot = {};
        snapshot.stock_code = fields[0];
        snapshot.exchange_code = fields[1];
        snapshot.date = std::stoul(fields[2]);
        snapshot.time = std::stoul(fields[3]);
        snapshot.price = parse_price_to_fen(fields[4]);
        snapshot.volume = parse_volume_to_100shares(fields[5]);
        snapshot.turnover = parse_turnover_to_fen(fields[6]);
        snapshot.trade_count = parse_volume_to_100shares(fields[7]);
        
        snapshot.high = parse_price_to_fen(fields[13]);
        snapshot.low = parse_price_to_fen(fields[14]);
        snapshot.open = parse_price_to_fen(fields[15]);
        snapshot.prev_close = parse_price_to_fen(fields[16]);
        
        // Parse ask prices (申卖价1-10: fields 17-26)
        for (int i = 0; i < 10; i++) {
            snapshot.ask_prices[i] = parse_price_to_fen(fields[17 + i]);
        }
        
        // Parse ask volumes (申卖量1-10: fields 27-36)
        for (int i = 0; i < 10; i++) {
            snapshot.ask_volumes[i] = parse_volume_to_100shares(fields[27 + i]);
        }
        
        // Parse bid prices (申买价1-10: fields 37-46)
        for (int i = 0; i < 10; i++) {
            snapshot.bid_prices[i] = parse_price_to_fen(fields[37 + i]);
        }
        
        // Parse bid volumes (申买量1-10: fields 47-56)
        for (int i = 0; i < 10; i++) {
            snapshot.bid_volumes[i] = parse_volume_to_100shares(fields[47 + i]);
        }
        
        snapshot.weighted_avg_ask_price = parse_price_to_fen(fields[57]);
        snapshot.weighted_avg_bid_price = parse_price_to_fen(fields[58]);
        snapshot.total_ask_volume = parse_volume_to_100shares(fields[59]);
        snapshot.total_bid_volume = parse_volume_to_100shares(fields[60]);
        
        snapshots.push_back(snapshot);
    }
    
    file.close();
    return true;
}

bool BinaryEncoder_L2::parse_order_csv(const std::string& filepath, std::vector<CSVOrder>& orders) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    std::string line;
    // Skip header line
    if (!std::getline(file, line)) {
        std::cerr << "Empty file: " << filepath << std::endl;
        return false;
    }
    
    while (std::getline(file, line)) {
        auto fields = split_csv_line(line);
        if (fields.size() < 10) {
            continue;
        }
        
        CSVOrder order = {};
        order.stock_code = fields[0];
        order.exchange_code = fields[1];
        order.date = std::stoul(fields[2]);
        order.time = std::stoul(fields[3]);
        order.order_id = std::stoull(fields[4]);
        order.exchange_order_id = std::stoull(fields[5]);
        
        // Handle order type
        if (!fields[6].empty()) {
            order.order_type = fields[6][0];
        } else {
            order.order_type = '0';
        }
        
        // Handle order side
        if (!fields[7].empty()) {
            order.order_side = fields[7][0];
        } else {
            order.order_side = ' ';
        }
        
        order.price = parse_price_to_fen(fields[8]);
        order.volume = parse_volume_to_100shares(fields[9]);
        
        orders.push_back(order);
    }
    
    file.close();
    return true;
}

bool BinaryEncoder_L2::parse_trade_csv(const std::string& filepath, std::vector<CSVTrade>& trades) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    std::string line;
    // Skip header line
    if (!std::getline(file, line)) {
        std::cerr << "Empty file: " << filepath << std::endl;
        return false;
    }
    
    while (std::getline(file, line)) {
        auto fields = split_csv_line(line);
        if (fields.size() < 12) {
            continue;
        }
        
        CSVTrade trade = {};
        trade.stock_code = fields[0];
        trade.exchange_code = fields[1];
        trade.date = std::stoul(fields[2]);
        trade.time = std::stoul(fields[3]);
        trade.trade_id = std::stoull(fields[4]);
        
        // Handle trade code
        if (!fields[5].empty()) {
            trade.trade_code = fields[5][0];
        } else {
            trade.trade_code = '0';
        }
        
        // Handle dummy code (field 6)
        if (!fields[6].empty()) {
            trade.dummy_code = fields[6][0];
        } else {
            trade.dummy_code = ' ';
        }
        
        // Handle BS flag
        if (!fields[7].empty()) {
            trade.bs_flag = fields[7][0];
        } else {
            trade.bs_flag = ' ';
        }
        
        trade.price = parse_price_to_fen(fields[8]);
        trade.volume = parse_volume_to_100shares(fields[9]);
        trade.ask_order_id = std::stoull(fields[10]);
        trade.bid_order_id = std::stoull(fields[11]);
        
        trades.push_back(trade);
    }
    
    file.close();
    return true;
}

// Private helper functions
uint8_t BinaryEncoder_L2::time_to_hour(uint32_t time_ms) {
    return static_cast<uint8_t>(time_ms / 3600000);
}

uint8_t BinaryEncoder_L2::time_to_minute(uint32_t time_ms) {
    return static_cast<uint8_t>((time_ms % 3600000) / 60000);
}

uint8_t BinaryEncoder_L2::time_to_second(uint32_t time_ms) {
    return static_cast<uint8_t>((time_ms % 60000) / 1000);
}

uint8_t BinaryEncoder_L2::time_to_millisecond_10ms(uint32_t time_ms) {
    return static_cast<uint8_t>((time_ms % 1000) / 10);
}

uint8_t BinaryEncoder_L2::determine_order_type(char csv_order_type, char /* csv_trade_code */, bool is_trade) {
    if (is_trade) {
        return 3; // taker (trade)
    }
    
    // For orders
    if (csv_order_type == 'A' || csv_order_type == '0') {
        return 0; // maker (order)
    } else if (csv_order_type == 'D') {
        return 1; // cancel
    }
    
    return 0; // default to maker
}

bool BinaryEncoder_L2::determine_order_direction(char side_flag) {
    return side_flag == 'S'; // 0:bid(B), 1:ask(S)
}

// CSV to L2 conversion functions
Snapshot BinaryEncoder_L2::csv_to_snapshot(const CSVSnapshot& csv_snap) {
    Snapshot snapshot = {};
    
    uint32_t time_ms = parse_time_to_ms(csv_snap.time);
    snapshot.hour = time_to_hour(time_ms);
    snapshot.minute = time_to_minute(time_ms);
    snapshot.second = time_to_second(time_ms);
    
    snapshot.trade_count = static_cast<uint8_t>(std::min(csv_snap.trade_count, 255u));
    snapshot.volume = static_cast<uint16_t>(std::min(csv_snap.volume, 65535u));
    snapshot.turnover = static_cast<uint32_t>(std::min(csv_snap.turnover, static_cast<uint64_t>(4294967295ULL)));
    
    // Convert prices from fen to our format (RMB * 100)
    snapshot.high = static_cast<uint16_t>(std::min(csv_snap.high, 65535u));
    snapshot.low = static_cast<uint16_t>(std::min(csv_snap.low, 65535u));
    snapshot.close = static_cast<uint16_t>(std::min(csv_snap.price, 65535u));
    
    // Copy bid/ask prices and volumes
    for (int i = 0; i < 10; i++) {
        snapshot.bid_price_ticks[i] = static_cast<uint16_t>(std::min(csv_snap.bid_prices[i], 65535u));
        snapshot.bid_volumes[i] = static_cast<uint16_t>(std::min(csv_snap.bid_volumes[i], 65535u));
        snapshot.ask_price_ticks[i] = static_cast<uint16_t>(std::min(csv_snap.ask_prices[i], 65535u));
        snapshot.ask_volumes[i] = static_cast<uint16_t>(std::min(csv_snap.ask_volumes[i], 65535u));
    }
    
    // Determine direction based on price movement (simplified)
    snapshot.direction = false; // Default to buy direction
    
    snapshot.all_bid_vwap = static_cast<uint16_t>(std::min(csv_snap.weighted_avg_bid_price, 65535u));
    snapshot.all_ask_vwap = static_cast<uint16_t>(std::min(csv_snap.weighted_avg_ask_price, 65535u));
    snapshot.all_bid_volume = static_cast<uint16_t>(std::min(csv_snap.total_bid_volume, 65535u));
    snapshot.all_ask_volume = static_cast<uint16_t>(std::min(csv_snap.total_ask_volume, 65535u));
    
    return snapshot;
}

Order BinaryEncoder_L2::csv_to_order(const CSVOrder& csv_order) {
    Order order = {};
    
    uint32_t time_ms = parse_time_to_ms(csv_order.time);
    order.hour = time_to_hour(time_ms);
    order.minute = time_to_minute(time_ms);
    order.second = time_to_second(time_ms);
    order.millisecond = time_to_millisecond_10ms(time_ms);
    
    order.order_type = determine_order_type(csv_order.order_type, '0', false);
    order.order_dir = determine_order_direction(csv_order.order_side);
    order.price = static_cast<uint16_t>(std::min(csv_order.price, 65535u));
    order.volume = static_cast<uint16_t>(std::min(csv_order.volume, 65535u));
    
    // Set order IDs based on direction and type
    if (order.order_dir == 0) { // bid
        order.bid_order_id = static_cast<uint32_t>(csv_order.order_id);
        order.ask_order_id = 0;
    } else { // ask
        order.bid_order_id = 0;
        order.ask_order_id = static_cast<uint32_t>(csv_order.order_id);
    }
    
    return order;
}

Order BinaryEncoder_L2::csv_to_trade_order(const CSVTrade& csv_trade) {
    Order order = {};
    
    uint32_t time_ms = parse_time_to_ms(csv_trade.time);
    order.hour = time_to_hour(time_ms);
    order.minute = time_to_minute(time_ms);
    order.second = time_to_second(time_ms);
    order.millisecond = time_to_millisecond_10ms(time_ms);
    
    order.order_type = determine_order_type('0', csv_trade.trade_code, true);
    order.order_dir = determine_order_direction(csv_trade.bs_flag);
    order.price = static_cast<uint16_t>(std::min(csv_trade.price, 65535u));
    order.volume = static_cast<uint16_t>(std::min(csv_trade.volume, 65535u));
    
    // For trades, set both order IDs
    order.bid_order_id = static_cast<uint32_t>(csv_trade.bid_order_id);
    order.ask_order_id = static_cast<uint32_t>(csv_trade.ask_order_id);
    
    return order;
}

// Binary encoding functions
bool BinaryEncoder_L2::encode_snapshots_to_binary(const std::vector<Snapshot>& snapshots,
                                              const std::string& filepath) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << filepath << std::endl;
        return false;
    }
    
    // Write header: number of snapshots
    size_t count = snapshots.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Write all snapshots
    for (const auto& snapshot : snapshots) {
        file.write(reinterpret_cast<const char*>(&snapshot), sizeof(snapshot));
    }
    
    file.close();
    return true;
}

bool BinaryEncoder_L2::encode_orders_to_binary(const std::vector<Order>& orders,
                                           const std::string& filepath) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << filepath << std::endl;
        return false;
    }
    
    // Write header: number of orders
    size_t count = orders.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Write all orders
    for (const auto& order : orders) {
        file.write(reinterpret_cast<const char*>(&order), sizeof(order));
    }
    
    file.close();
    return true;
}

// High-level processing function
bool BinaryEncoder_L2::process_stock_data(const std::string& stock_dir,
                                       const std::string& output_dir,
                                       const std::string& stock_code) {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_dir);
    
    std::vector<CSVSnapshot> csv_snapshots;
    std::vector<CSVOrder> csv_orders;
    std::vector<CSVTrade> csv_trades;
    
    // Parse CSV files
    std::string snapshot_file = stock_dir + "/行情.csv";
    std::string order_file = stock_dir + "/逐笔委托.csv";
    std::string trade_file = stock_dir + "/逐笔成交.csv";
    
    // Parse snapshots
    if (std::filesystem::exists(snapshot_file)) {
        if (!parse_snapshot_csv(snapshot_file, csv_snapshots)) {
            std::cerr << "Failed to parse snapshot file: " << snapshot_file << std::endl;
            return false;
        }
        std::cout << "Parsed " << csv_snapshots.size() << " snapshots from " << snapshot_file << std::endl;
    }
    
    // Parse orders
    if (std::filesystem::exists(order_file)) {
        if (!parse_order_csv(order_file, csv_orders)) {
            std::cerr << "Failed to parse order file: " << order_file << std::endl;
            return false;
        }
        std::cout << "Parsed " << csv_orders.size() << " orders from " << order_file << std::endl;
    }
    
    // Parse trades
    if (std::filesystem::exists(trade_file)) {
        if (!parse_trade_csv(trade_file, csv_trades)) {
            std::cerr << "Failed to parse trade file: " << trade_file << std::endl;
            return false;
        }
        std::cout << "Parsed " << csv_trades.size() << " trades from " << trade_file << std::endl;
    }
    
    // Convert and encode snapshots
    if (!csv_snapshots.empty()) {
        std::vector<Snapshot> snapshots;
        for (const auto& csv_snap : csv_snapshots) {
            snapshots.push_back(csv_to_snapshot(csv_snap));
        }
        
        std::string output_file = output_dir + "/" + stock_code + "_snapshots.bin";
        if (!encode_snapshots_to_binary(snapshots, output_file)) {
            std::cerr << "Failed to encode snapshots to " << output_file << std::endl;
            return false;
        }
        std::cout << "Encoded " << snapshots.size() << " snapshots to " << output_file << std::endl;
    }
    
    // Convert and encode orders and trades together
    std::vector<Order> all_orders;
    
    // Add orders
    for (const auto& csv_order : csv_orders) {
        all_orders.push_back(csv_to_order(csv_order));
    }
    
    // Add trades as taker orders
    for (const auto& csv_trade : csv_trades) {
        all_orders.push_back(csv_to_trade_order(csv_trade));
    }
    
    // Sort orders by time
    std::sort(all_orders.begin(), all_orders.end(), [](const Order& a, const Order& b) {
        uint32_t time_a = a.hour * 3600000 + a.minute * 60000 + a.second * 1000 + a.millisecond * 10;
        uint32_t time_b = b.hour * 3600000 + b.minute * 60000 + b.second * 1000 + b.millisecond * 10;
        return time_a < time_b;
    });
    
    if (!all_orders.empty()) {
        std::string output_file = output_dir + "/" + stock_code + "_orders.bin";
        if (!encode_orders_to_binary(all_orders, output_file)) {
            std::cerr << "Failed to encode orders to " << output_file << std::endl;
            return false;
        }
        std::cout << "Encoded " << all_orders.size() << " orders to " << output_file << std::endl;
    }
    
    return true;
}

} // namespace L2
