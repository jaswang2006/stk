#include "codec/binary_decoder_L2.hpp"
#include "codec/delta_encoding.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>

namespace L2 {

// Constructor with capacity hints
BinaryDecoder_L2::BinaryDecoder_L2(size_t estimated_snapshots, size_t estimated_orders) {
    // Pre-reserve space for snapshot vectors
    temp_hours.reserve(estimated_snapshots);
    temp_minutes.reserve(estimated_snapshots);
    temp_seconds.reserve(estimated_snapshots);
    temp_highs.reserve(estimated_snapshots);
    temp_lows.reserve(estimated_snapshots);
    temp_closes.reserve(estimated_snapshots);
    temp_all_bid_vwaps.reserve(estimated_snapshots);
    temp_all_ask_vwaps.reserve(estimated_snapshots);
    temp_all_bid_volumes.reserve(estimated_snapshots);
    temp_all_ask_volumes.reserve(estimated_snapshots);
    
    for (size_t i = 0; i < 10; ++i) {
        temp_bid_prices[i].reserve(estimated_snapshots);
        temp_ask_prices[i].reserve(estimated_snapshots);
    }
    
    // Pre-reserve space for order vectors
    temp_order_hours.reserve(estimated_orders);
    temp_order_minutes.reserve(estimated_orders);
    temp_order_seconds.reserve(estimated_orders);
    temp_order_milliseconds.reserve(estimated_orders);
    temp_order_prices.reserve(estimated_orders);
    temp_bid_order_ids.reserve(estimated_orders);
    temp_ask_order_ids.reserve(estimated_orders);
}

size_t BinaryDecoder_L2::extract_count_from_filename(const std::string &filepath) {
  // Extract count from filename pattern: *_snapshots_<count>.bin or *_orders_<count>.bin
  std::filesystem::path file_path(filepath);
  std::string filename = file_path.stem().string(); // Get filename without extension

  // Look for pattern _<number> at the end
  std::regex count_regex(R"(_(\d+)$)");
  std::smatch match;

  if (std::regex_search(filename, match, count_regex)) {
    return std::stoull(match[1].str());
  }

  return 0; // Return 0 if count cannot be extracted
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

inline double BinaryDecoder_L2::price_to_rmb(uint16_t price_ticks) {
  return static_cast<double>(price_ticks) * 0.01; // Convert from 0.01 RMB units to RMB
}

inline double BinaryDecoder_L2::vwap_to_rmb(uint16_t vwap_ticks) {
  return static_cast<double>(vwap_ticks) * 0.001; // Convert from 0.001 RMB units to RMB
}

inline uint32_t BinaryDecoder_L2::volume_to_shares(uint16_t volume_100shares) {
  return static_cast<uint32_t>(volume_100shares) * 100;
}

const char *BinaryDecoder_L2::order_type_to_string(uint8_t order_type) {
  switch (order_type) {
  case 0:
    return "MAKER";
  case 1:
    return "CANCEL";
  case 2:
    return "CHANGE";
  case 3:
    return "TAKER";
  default:
    return "UNKNOWN";
  }
}

const char *BinaryDecoder_L2::order_dir_to_string(uint8_t order_dir) {
  return order_dir == 0 ? "BID" : "ASK";
}

void BinaryDecoder_L2::print_snapshot(const Snapshot &snapshot, size_t index) {
  std::cout << "=== Snapshot " << index << " ===" << std::endl;
  std::cout << "Time: " << time_to_string(snapshot.hour, snapshot.minute, snapshot.second) << std::endl;
  std::cout << "Close: " << std::fixed << std::setprecision(2) << price_to_rmb(snapshot.close) << " RMB" << std::endl;
  std::cout << "High: " << price_to_rmb(snapshot.high) << " RMB" << std::endl;
  std::cout << "Low: " << price_to_rmb(snapshot.low) << " RMB" << std::endl;
  std::cout << "Volume: " << volume_to_shares(snapshot.volume) << " shares" << std::endl;
  std::cout << "Turnover: " << snapshot.turnover << " fen" << std::endl;
  std::cout << "Trade Count (incremental): " << static_cast<int>(snapshot.trade_count) << std::endl;

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

  std::cout << "VWAP - Bid: " << vwap_to_rmb(snapshot.all_bid_vwap)
            << ", Ask: " << vwap_to_rmb(snapshot.all_ask_vwap) << std::endl;
  std::cout << "Total Volume - Bid: " << volume_to_shares(snapshot.all_bid_volume)
            << ", Ask: " << volume_to_shares(snapshot.all_ask_volume) << std::endl;
  std::cout << std::endl;
}

void BinaryDecoder_L2::print_order(const Order &order, size_t index) {
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

void BinaryDecoder_L2::print_all_snapshots(const std::vector<Snapshot> &snapshots) {
  // hr mn sc trd   vol   turnover  high   low close   bp0   bp1   bp2   bp3   bp4   bp5   bp6   bp7   bp8   bp9   bv0   bv1   bv2   bv3   bv4   bv5   bv6   bv7   bv8   bv9   ap0   ap1   ap2   ap3   ap4   ap5   ap6   ap7   ap8   ap9   av0   av1   av2   av3   av4   av5   av6   av7   av8   av9 d b_vwp a_vwp b_vol a_vol
  // 14 48 45 157     2       1320   665   660   660   660   659   658   657   656   655   654   653   652   651  3854  3831  2879  4500  1294   813   301  1449   714  1236   661   662   663   664   665   666   667   668   669   670  4094  4526  3664  4811  7374  3123  3026  2396  3277  3513 0  6339  6864 53296 65535
  std::cout << "=== All Snapshots ===" << std::endl;

  // Print aligned header using compile-time bit width calculations
  using namespace BitWidthFormat;

  std::cout << std::setw(hour_width()) << std::right << "hr" << " "
            << std::setw(minute_width()) << std::right << "mn" << " "
            << std::setw(second_width()) << std::right << "sc" << " "
            << std::setw(trade_count_width()) << std::right << "trd" << " "
            << std::setw(volume_width()) << std::right << "vol" << " "
            << std::setw(turnover_width()) << std::right << "turnover" << " "
            << std::setw(price_width()) << std::right << "high" << " "
            << std::setw(price_width()) << std::right << "low" << " "
            << std::setw(price_width()) << std::right << "close" << " ";

  // bid_price_ticks[10] - using price bit width from schema
  for (int i = 0; i < 10; i++) {
    std::cout << std::setw(price_width()) << std::right << ("bp" + std::to_string(i)) << " ";
  }

  // bid_volumes[10] - using volume bit width from schema
  for (int i = 0; i < 10; i++) {
    std::cout << std::setw(get_column_width("bid_volumes[10]")) << std::right << ("bv" + std::to_string(i)) << " ";
  }

  // ask_price_ticks[10] - using price bit width from schema
  for (int i = 0; i < 10; i++) {
    std::cout << std::setw(price_width()) << std::right << ("ap" + std::to_string(i)) << " ";
  }

  // ask_volumes[10] - using volume bit width from schema
  for (int i = 0; i < 10; i++) {
    std::cout << std::setw(get_column_width("ask_volumes[10]")) << std::right << ("av" + std::to_string(i)) << " ";
  }

  std::cout << std::setw(direction_width()) << std::right << "d" << " "
            << std::setw(vwap_width()) << std::right << "b_vwp" << " "
            << std::setw(vwap_width()) << std::right << "a_vwp" << " "
            << std::setw(total_volume_width()) << std::right << "b_vol" << " "
            << std::setw(total_volume_width()) << std::right << "a_vol" << std::endl;

  // Print data rows with aligned formatting using compile-time bit width calculations
  for (const auto &snapshot : snapshots) {
    std::cout << std::setw(hour_width()) << std::right << static_cast<int>(snapshot.hour) << " "
              << std::setw(minute_width()) << std::right << static_cast<int>(snapshot.minute) << " "
              << std::setw(second_width()) << std::right << static_cast<int>(snapshot.second) << " "
              << std::setw(trade_count_width()) << std::right << static_cast<int>(snapshot.trade_count) << " "
              << std::setw(volume_width()) << std::right << snapshot.volume << " "
              << std::setw(turnover_width()) << std::right << snapshot.turnover << " "
              << std::setw(price_width()) << std::right << snapshot.high << " "
              << std::setw(price_width()) << std::right << snapshot.low << " "
              << std::setw(price_width()) << std::right << snapshot.close << " ";

    // Output bid_price_ticks[10] - using price bit width from schema
    for (int i = 0; i < 10; i++) {
      std::cout << std::setw(price_width()) << std::right << snapshot.bid_price_ticks[i] << " ";
    }

    // Output bid_volumes[10] - using volume bit width from schema
    for (int i = 0; i < 10; i++) {
      std::cout << std::setw(get_column_width("bid_volumes[10]")) << std::right << snapshot.bid_volumes[i] << " ";
    }

    // Output ask_price_ticks[10] - using price bit width from schema
    for (int i = 0; i < 10; i++) {
      std::cout << std::setw(price_width()) << std::right << snapshot.ask_price_ticks[i] << " ";
    }

    // Output ask_volumes[10] - using volume bit width from schema
    for (int i = 0; i < 10; i++) {
      std::cout << std::setw(get_column_width("ask_volumes[10]")) << std::right << snapshot.ask_volumes[i] << " ";
    }

    std::cout << std::setw(direction_width()) << std::right << (snapshot.direction ? 1 : 0) << " "
              << std::setw(vwap_width()) << std::right << snapshot.all_bid_vwap << " "
              << std::setw(vwap_width()) << std::right << snapshot.all_ask_vwap << " "
              << std::setw(total_volume_width()) << std::right << snapshot.all_bid_volume << " "
              << std::setw(total_volume_width()) << std::right << snapshot.all_ask_volume << std::endl;
  }
}

void BinaryDecoder_L2::print_all_orders(const std::vector<Order> &orders) {

  // hr mn sc  ms t d price   vol bid_ord_id ask_ord_id
  // 9 15  0   2 0 0   601     1     137525          0
  // 9 15  0   2 0 1   727     1          0     137524

  // Print aligned header using compile-time bit width calculations
  using namespace BitWidthFormat;

  std::cout << std::setw(hour_width()) << std::right << "hr" << " "
            << std::setw(minute_width()) << std::right << "mn" << " "
            << std::setw(second_width()) << std::right << "sc" << " "
            << std::setw(millisecond_width()) << std::right << "ms" << " "
            << std::setw(order_type_width()) << std::right << "t" << " "
            << std::setw(order_dir_width()) << std::right << "d" << " "
            << std::setw(order_price_width()) << std::right << "price" << " "
            << std::setw(order_volume_width()) << std::right << "vol" << " "
            << std::setw(order_id_width()) << std::right << "bid_ord_id" << " "
            << std::setw(order_id_width()) << std::right << "ask_ord_id" << std::endl;

  // Print data rows with aligned formatting using compile-time bit width calculations
  for (const auto &order : orders) {
    std::cout << std::setw(hour_width()) << std::right << static_cast<int>(order.hour) << " "
              << std::setw(minute_width()) << std::right << static_cast<int>(order.minute) << " "
              << std::setw(second_width()) << std::right << static_cast<int>(order.second) << " "
              << std::setw(millisecond_width()) << std::right << static_cast<int>(order.millisecond) << " "
              << std::setw(order_type_width()) << std::right << static_cast<int>(order.order_type) << " "
              << std::setw(order_dir_width()) << std::right << static_cast<int>(order.order_dir) << " "
              << std::setw(order_price_width()) << std::right << order.price << " "
              << std::setw(order_volume_width()) << std::right << order.volume << " "
              << std::setw(order_id_width()) << std::right << order.bid_order_id << " "
              << std::setw(order_id_width()) << std::right << order.ask_order_id << std::endl;
  }
}

// decoder functions
bool BinaryDecoder_L2::decode_snapshots(const std::string& filepath, std::vector<Snapshot>& snapshots, bool use_delta) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) [[unlikely]] {
    std::cerr << "L2 Decoder: Failed to open snapshot file: " << filepath << std::endl;
    return false;
  }
  
  // Read simple header: count
  size_t count;
  file.read(reinterpret_cast<char*>(&count), sizeof(count));
  
  if (file.fail()) [[unlikely]] {
    std::cerr << "L2 Decoder: Failed to read header: " << filepath << std::endl;
    return false;
  }
  
  // Pre-allocate exact size for optimal memory usage
  snapshots.resize(count);
  
  // Read snapshots directly
  file.read(reinterpret_cast<char*>(snapshots.data()), count * sizeof(Snapshot));
  
  if (file.fail()) [[unlikely]] {
    std::cerr << "L2 Decoder: Failed to read snapshot data: " << filepath << std::endl;
    return false;
  }
  
  // Decode deltas (reverse the delta encoding process)
  if (use_delta && count > 1) {
    std::cout << "L2 Decoder: Applying delta decoding to " << count << " snapshots..." << std::endl;
    // Reuse pre-allocated member vectors for better performance
    temp_hours.resize(count);
    temp_minutes.resize(count);
    temp_seconds.resize(count);
    temp_highs.resize(count);
    temp_lows.resize(count);
    temp_closes.resize(count);
    temp_all_bid_vwaps.resize(count);
    temp_all_ask_vwaps.resize(count);
    temp_all_bid_volumes.resize(count);
    temp_all_ask_volumes.resize(count);
    
    for (size_t i = 0; i < 10; ++i) {
      temp_bid_prices[i].resize(count);
      temp_ask_prices[i].resize(count);
    }
    
    // Extract delta-encoded data
    for (size_t i = 0; i < count; ++i) {
      temp_hours[i] = snapshots[i].hour;
      temp_minutes[i] = snapshots[i].minute;
      temp_seconds[i] = snapshots[i].second;
      temp_highs[i] = snapshots[i].high;
      temp_lows[i] = snapshots[i].low;
      temp_closes[i] = snapshots[i].close;
      temp_all_bid_vwaps[i] = snapshots[i].all_bid_vwap;
      temp_all_ask_vwaps[i] = snapshots[i].all_ask_vwap;
      temp_all_bid_volumes[i] = snapshots[i].all_bid_volume;
      temp_all_ask_volumes[i] = snapshots[i].all_ask_volume;
      
      for (size_t j = 0; j < 10; ++j) {
        temp_bid_prices[j][i] = snapshots[i].bid_price_ticks[j];
        temp_ask_prices[j][i] = snapshots[i].ask_price_ticks[j];
      }
    }
    
    // Decode deltas where use_delta=true (reverse of encode_deltas)
    DeltaUtils::decode_deltas(temp_hours.data(), count);
    DeltaUtils::decode_deltas(temp_minutes.data(), count);
    DeltaUtils::decode_deltas(temp_seconds.data(), count);
    DeltaUtils::decode_deltas(temp_highs.data(), count);
    DeltaUtils::decode_deltas(temp_lows.data(), count);
    DeltaUtils::decode_deltas(temp_closes.data(), count);
    DeltaUtils::decode_deltas(temp_all_bid_vwaps.data(), count);
    DeltaUtils::decode_deltas(temp_all_ask_vwaps.data(), count);
    DeltaUtils::decode_deltas(temp_all_bid_volumes.data(), count);
    DeltaUtils::decode_deltas(temp_all_ask_volumes.data(), count);
    
    for (size_t j = 0; j < 10; ++j) {
      DeltaUtils::decode_deltas(temp_bid_prices[j].data(), count);
      DeltaUtils::decode_deltas(temp_ask_prices[j].data(), count);
    }
    
    // Copy back decoded data
    for (size_t i = 0; i < count; ++i) {
      snapshots[i].hour = temp_hours[i];
      snapshots[i].minute = temp_minutes[i];
      snapshots[i].second = temp_seconds[i];
      snapshots[i].high = temp_highs[i];
      snapshots[i].low = temp_lows[i];
      snapshots[i].close = temp_closes[i];
      snapshots[i].all_bid_vwap = temp_all_bid_vwaps[i];
      snapshots[i].all_ask_vwap = temp_all_ask_vwaps[i];
      snapshots[i].all_bid_volume = temp_all_bid_volumes[i];
      snapshots[i].all_ask_volume = temp_all_ask_volumes[i];
      
      for (size_t j = 0; j < 10; ++j) {
        snapshots[i].bid_price_ticks[j] = temp_bid_prices[j][i];
        snapshots[i].ask_price_ticks[j] = temp_ask_prices[j][i];
      }
    }
  }
  
  std::cout << "L2 Decoder: Successfully decoded " << snapshots.size() <<" snapshots from " << filepath << std::endl;
  
  return true;
}

bool BinaryDecoder_L2::decode_orders(const std::string& filepath, std::vector<Order>& orders, bool use_delta) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) [[unlikely]] {
    std::cerr << "L2 Decoder: Failed to open order file: " << filepath << std::endl;
    return false;
  }
  
  // Read simple header: count
  size_t count;
  file.read(reinterpret_cast<char*>(&count), sizeof(count));
  
  if (file.fail()) [[unlikely]] {
    std::cerr << "L2 Decoder: Failed to read header: " << filepath << std::endl;
    return false;
  }
  
  // Pre-allocate exact size for optimal memory usage
  orders.resize(count);
  
  // Read orders directly
  file.read(reinterpret_cast<char*>(orders.data()), count * sizeof(Order));
  
  if (file.fail()) [[unlikely]] {
    std::cerr << "L2 Decoder: Failed to read order data: " << filepath << std::endl;
    return false;
  }
  
  // Decode deltas (reverse the delta encoding process)
  if (use_delta && count > 1) {
    std::cout << "L2 Decoder: Applying delta decoding to " << count << " orders..." << std::endl;
    // Reuse pre-allocated member vectors for better performance
    temp_order_hours.resize(count);
    temp_order_minutes.resize(count);
    temp_order_seconds.resize(count);
    temp_order_milliseconds.resize(count);
    temp_order_prices.resize(count);
    temp_bid_order_ids.resize(count);
    temp_ask_order_ids.resize(count);
    
    // Extract delta-encoded data
    for (size_t i = 0; i < count; ++i) {
      temp_order_hours[i] = orders[i].hour;
      temp_order_minutes[i] = orders[i].minute;
      temp_order_seconds[i] = orders[i].second;
      temp_order_milliseconds[i] = orders[i].millisecond;
      temp_order_prices[i] = orders[i].price;
      temp_bid_order_ids[i] = orders[i].bid_order_id;
      temp_ask_order_ids[i] = orders[i].ask_order_id;
    }
    
    // Decode deltas where use_delta=true (reverse of encode_deltas)
    DeltaUtils::decode_deltas(temp_order_hours.data(), count);
    DeltaUtils::decode_deltas(temp_order_minutes.data(), count);
    DeltaUtils::decode_deltas(temp_order_seconds.data(), count);
    DeltaUtils::decode_deltas(temp_order_milliseconds.data(), count);
    DeltaUtils::decode_deltas(temp_order_prices.data(), count);
    DeltaUtils::decode_deltas(temp_bid_order_ids.data(), count);
    DeltaUtils::decode_deltas(temp_ask_order_ids.data(), count);
    
    // Copy back decoded data
    for (size_t i = 0; i < count; ++i) {
      orders[i].hour = temp_order_hours[i];
      orders[i].minute = temp_order_minutes[i];
      orders[i].second = temp_order_seconds[i];
      orders[i].millisecond = temp_order_milliseconds[i];
      orders[i].price = temp_order_prices[i];
      orders[i].bid_order_id = temp_bid_order_ids[i];
      orders[i].ask_order_id = temp_ask_order_ids[i];
    }
  }
  
  std::cout << "L2 Decoder: Successfully decoded " << orders.size() << " orders from " << filepath << std::endl;
  
  return true;
}

} // namespace L2
