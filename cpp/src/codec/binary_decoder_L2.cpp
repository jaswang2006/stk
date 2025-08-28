#include "codec/binary_decoder_L2.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>

/*
 * Data Field Definitions for L2 Market Data Structures:
 *
 * Snapshot struct fields:
 * - hour: Time hour component (0-23, 5bit)
 * - minute: Time minute component (0-59, 6bit)
 * - second: Time second component (0-59, 6bit)
 * - trade_count: Number of trades in this snapshot (8bit)
 * - volume: Trading volume in units of 100 shares (16bit)
 * - turnover: Trading turnover in RMB (32bit)
 * - high: Highest price in 0.01 RMB units (14bit price ticks)
 * - low: Lowest price in 0.01 RMB units (14bit price ticks)
 * - close: Closing price in 0.01 RMB units (14bit price ticks)
 * - bid_price_ticks[10]: 10 bid price levels in 0.01 RMB units (14bit each)
 * - bid_volumes[10]: 10 bid volume levels in units of 100 shares (14bit each)
 * - ask_price_ticks[10]: 10 ask price levels in 0.01 RMB units (14bit each)
 * - ask_volumes[10]: 10 ask volume levels in units of 100 shares (14bit each)
 * - direction: Price movement direction (1bit: 0=buy, 1=sell)
 * - all_bid_vwap: Volume-weighted average price of all bid orders in 0.001 RMB units (15bit)
 * - all_ask_vwap: Volume-weighted average price of all ask orders in 0.001 RMB units (15bit)
 * - all_bid_volume: Total volume of all bid orders in units of 100 shares (14bit)
 * - all_ask_volume: Total volume of all ask orders in units of 100 shares (14bit)
 *
 * Order struct fields:
 * - hour: Time hour component (0-23, 5bit)
 * - minute: Time minute component (0-59, 6bit)
 * - second: Time second component (0-59, 6bit)
 * - millisecond: Time millisecond component in 10ms units (7bit)
 * - order_type: Order operation type (2bit: 0=maker, 1=cancel, 2=change, 3=taker)
 * - order_dir: Order direction (1bit: 0=bid, 1=ask)
 * - price: Order price in 0.01 RMB units (14bit price ticks)
 * - volume: Order volume in units of 100 shares (16bit)
 * - bid_order_id: Buy order identifier (32bit)
 * - ask_order_id: Sell order identifier (32bit)
 */

namespace L2 {

bool BinaryDecoder_L2::decode_snapshots_from_binary(const std::string &filepath,
                                                    std::vector<Snapshot> &snapshots) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open binary file: " << filepath << std::endl;
    return false;
  }

  // Read header: number of snapshots
  size_t count;
  file.read(reinterpret_cast<char *>(&count), sizeof(count));
  if (file.fail()) {
    std::cerr << "Failed to read snapshot count from " << filepath << std::endl;
    return false;
  }

  snapshots.reserve(count);

  // Read all snapshots
  for (size_t i = 0; i < count; i++) {
    Snapshot snapshot;
    file.read(reinterpret_cast<char *>(&snapshot), sizeof(snapshot));
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

bool BinaryDecoder_L2::decode_orders_from_binary(const std::string &filepath,
                                                 std::vector<Order> &orders) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open binary file: " << filepath << std::endl;
    return false;
  }

  // Read header: number of orders
  size_t count;
  file.read(reinterpret_cast<char *>(&count), sizeof(count));
  if (file.fail()) {
    std::cerr << "Failed to read order count from " << filepath << std::endl;
    return false;
  }

  orders.reserve(count);

  // Read all orders
  for (size_t i = 0; i < count; i++) {
    Order order;
    file.read(reinterpret_cast<char *>(&order), sizeof(order));
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
  return static_cast<double>(price_ticks) * 0.01; // Convert from 0.01 RMB units to RMB
}

double BinaryDecoder_L2::vwap_to_rmb(uint16_t vwap_ticks) {
  return static_cast<double>(vwap_ticks) * 0.001; // Convert from 0.001 RMB units to RMB
}

uint32_t BinaryDecoder_L2::volume_to_shares(uint16_t volume_100shares) {
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
    std::cout << std::setw(calc_width(Snapshot_Schema[11].bit_width)) << std::right << ("bv" + std::to_string(i)) << " ";
  }

  // ask_price_ticks[10] - using price bit width from schema
  for (int i = 0; i < 10; i++) {
    std::cout << std::setw(price_width()) << std::right << ("ap" + std::to_string(i)) << " ";
  }

  // ask_volumes[10] - using volume bit width from schema
  for (int i = 0; i < 10; i++) {
    std::cout << std::setw(calc_width(Snapshot_Schema[12].bit_width)) << std::right << ("av" + std::to_string(i)) << " ";
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
      std::cout << std::setw(calc_width(Snapshot_Schema[11].bit_width)) << std::right << snapshot.bid_volumes[i] << " ";
    }

    // Output ask_price_ticks[10] - using price bit width from schema
    for (int i = 0; i < 10; i++) {
      std::cout << std::setw(price_width()) << std::right << snapshot.ask_price_ticks[i] << " ";
    }

    // Output ask_volumes[10] - using volume bit width from schema
    for (int i = 0; i < 10; i++) {
      std::cout << std::setw(calc_width(Snapshot_Schema[12].bit_width)) << std::right << snapshot.ask_volumes[i] << " ";
    }

    std::cout << std::setw(direction_width()) << std::right << (snapshot.direction ? 1 : 0) << " "
              << std::setw(vwap_width()) << std::right << snapshot.all_bid_vwap << " "
              << std::setw(vwap_width()) << std::right << snapshot.all_ask_vwap << " "
              << std::setw(total_volume_width()) << std::right << snapshot.all_bid_volume << " "
              << std::setw(total_volume_width()) << std::right << snapshot.all_ask_volume << std::endl;
  }
}

void BinaryDecoder_L2::print_all_orders(const std::vector<Order> &orders) {
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

} // namespace L2
