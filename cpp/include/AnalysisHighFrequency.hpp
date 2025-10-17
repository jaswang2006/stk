#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "codec/L2_DataType.hpp"
#include "define/MemPool.hpp"
// #include "features/backend/FeaturesConfig.hpp"
#include "math/sample/ResampleRunBar.hpp"

//========================================================================================
// CONFIGURATION PARAMETERS
//========================================================================================

// Debug switches
#define DEBUG_ORDER_PRINT 0        // Print every order processing
#define DEBUG_BOOK_PRINT 1         // Print order book snapshot
#define DEBUG_BOOK_BY_SECOND 1     // 0: by tick, 1: every 1 second, 2: every 2 seconds, ...
#define DEBUG_BOOK_AS_AMOUNT 1     // 0: 手, 1: 1万元, 2: 2万元, 3: 3万元, ...
#define DEBUG_ANOMALY_PRINT 1      // Print max unmatched order with creation timestamp
#define DEBUG_DEFERRED_ENQUEUE 1   // Print when order is enqueued (入队) to deferred_queue_
#define DEBUG_DEFERRED_FLUSH 1     // Print when order is flushed (出队/处理) from deferred_queue_
#define DEBUG_SINGLE_DAY 1         // Exit after processing one day

// Trading session time points (China A-share market)
namespace TradingSession {
  // Morning call auction (集合竞价)
  constexpr uint8_t MORNING_CALL_AUCTION_START_HOUR = 9;
  constexpr uint8_t MORNING_CALL_AUCTION_START_MINUTE = 15;
  constexpr uint8_t MORNING_CALL_AUCTION_END_MINUTE = 25;
  
  // Morning matching period (集合竞价撮合期)
  constexpr uint8_t MORNING_MATCHING_START_MINUTE = 25;
  constexpr uint8_t MORNING_MATCHING_END_MINUTE = 30;
  
  // Continuous auction (连续竞价)
  constexpr uint8_t CONTINUOUS_TRADING_START_HOUR = 9;
  constexpr uint8_t CONTINUOUS_TRADING_START_MINUTE = 30;
  constexpr uint8_t CONTINUOUS_TRADING_END_HOUR = 15;
  constexpr uint8_t CONTINUOUS_TRADING_END_MINUTE = 0;
  
  // Closing call auction (收盘集合竞价 - Shenzhen only)
  constexpr uint8_t CLOSING_CALL_AUCTION_START_HOUR = 14;
  constexpr uint8_t CLOSING_CALL_AUCTION_START_MINUTE = 57;
  constexpr uint8_t CLOSING_CALL_AUCTION_END_HOUR = 15;
  constexpr uint8_t CLOSING_CALL_AUCTION_END_MINUTE = 0;
}

// LOB visualization parameters
namespace BookDisplay {
  constexpr size_t MAX_DISPLAY_LEVELS = 10;  // Number of price levels to display
  constexpr size_t LEVEL_WIDTH = 12;         // Width for each price level display
}

// Anomaly detection parameters
namespace AnomalyDetection {
  constexpr uint16_t MIN_DISTANCE_FROM_TOB = 5;  // Minimum distance from TOB to check anomalies
}

//========================================================================================
// ORDER TYPE PROCESSING COMPARISON TABLE
//========================================================================================
//
// ┌─────────────────────────┬──────────────────────────────┬──────────────────────────────┬──────────────────────────────┐
// │ Dimension               │ MAKER (Creator·Consumer)     │ TAKER (Counterparty·TOB)     │ CANCEL (Self·Shenzhen no px) │
// ├─────────────────────────┼──────────────────────────────┼──────────────────────────────┼──────────────────────────────┤
// │ target_id               │ Self ID                      │ Counterparty ID (reversed!)  │ Self ID                      │
// │ signed_volume           │ BID: +  ASK: -               │ BID: +  ASK: -               │ BID: -  ASK: +               │
// ├─────────────────────────┼──────────────────────────────┼──────────────────────────────┼──────────────────────────────┤
// │ order_lookup_ access    │ [Write] Create/Update Loc    │ [R/W] Update/Erase Loc       │ [R/W] Update/Erase Loc       │
// │ deferred_queue_         │ [Read+Del] Check & flush     │ [Write] If not found         │ [Write] If not found         │
// │ order_memory_pool_      │ [Alloc] Common               │ [-] Rare (out-of-order)      │ [-] Rare (out-of-order)      │
// │ level_storage_          │ [Create] May create Level    │ [-] Never                    │ [-] Never                    │
// │ visible_price_bitmap_   │ add/remove                   │ May remove                   │ May remove                   │
// │ best_bid_/best_ask_     │ [-]                          │ [YES] Update TOB             │ [-]                          │
// ├─────────────────────────┼──────────────────────────────┼──────────────────────────────┼──────────────────────────────┤
// │ Level::net_quantity     │ Increase (+/-)               │ Decrease                     │ Decrease                     │
// │ When order found        │ qty += signed_vol (merge)    │ qty += signed_vol (deduct C) │ qty += signed_vol (deduct S) │
// │ When order NOT found    │ Flush deferred + Create      │ Enqueue to deferred          │ Enqueue to deferred          │
// │ price=0 handling        │ Defer to queue (special)     │ Use level->price             │ Common (SZ), use level->price│
// ├─────────────────────────┼──────────────────────────────┼──────────────────────────────┼──────────────────────────────┤
// │ Hash lookups (hot path) │ 1x (order_lookup_ only)      │ 1x (order_lookup_ only)      │ 1x (order_lookup_ only)      │
// │ Hash lookups (deferred) │ 2x (+ deferred_queue_)       │ 2x (+ deferred_queue_)       │ 2x (+ deferred_queue_)       │
// │ Typical probability     │ 98% hot path, 2% deferred    │ 98% hot path, 2% deferred    │ 95% hot path, 5% deferred    │
// ├─────────────────────────┼──────────────────────────────┼──────────────────────────────┼──────────────────────────────┤
// │ Core characteristics    │ Create order, consume defer, │ Operate counterparty, prod   │ Operate self, produce defer, │
// │                         │ allocate memory (creator)    │ defer, update TOB (reversed!)│ no price (Shenzhen special)  │
// └─────────────────────────┴──────────────────────────────┴──────────────────────────────┴──────────────────────────────┘
//
//========================================================================================

// 我们采用抵扣模型 + 统一延迟队列 (simple & robust & performant)
//
//========================================================================================
// 数据问题与corner cases:
//========================================================================================
// 1. 乱序 (Out-of-Order): 同一ms内(甚至不同时刻间), order之间可能为乱序
//    - TAKER/CANCEL可能先于对应MAKER到达 (~2-5%概率)
// 2. 集合竞价 (Call Auction): 9:15-9:30, 14:57-15:00期间MAKER需特殊处理
//    - 9:15-9:25: 集合竞价期, MAKER报价不是真实成交价
//    - 9:25-9:30: 集合竞价撮合期, TAKER在队列中找MAKER抵扣
//    - 9:30:00: 剩余MAKER按挂单价flush到LOB
// 3. 特殊挂单 (Special MAKER): 市价单('1')和本方最优('U')可能无价格 (price=0)
//    - 需等待TAKER提供成交价 (~1-2%概率)
// 4. 零价格撤单 (Zero-Price Cancel): 深交所撤单无价格信息 (price=0)
//    - 需等待MAKER提供价格或使用LOB现有价格 (~5-10%撤单概率)
// 5. 快照异步 (Snapshot Async): 快照的时间点不确定, 只能作为模糊矫正
// 6. 数据丢失 (Data Loss): order信息可能丢失 (交易所问题, 无法修复)
//
//========================================================================================
// 核心设计: 抵扣模型 + 统一延迟队列
//========================================================================================
//
// 订单处理优先级: deferred_queue_ (Queue优先) > order_lookup_ (LOB) > 特殊处理
// 原因: 乱序很常见(~2-5%), queue中的订单需要第一时间被处理
//
// HOT PATH (96%+ orders): 连续竞价期间正常订单, queue空
// --------------------------------------------------------
// - MAKER: 直接创建订单到LOB (order_lookup_仅1次查找)
// - TAKER: 找到对手MAKER并抵扣 (order_lookup_仅1次查找)
// - CANCEL: 找到自身MAKER并抵扣 (order_lookup_仅1次查找)
// - 性能优先: 无额外判断, 无deferred_queue_查找, 最快路径
//
// DEFERRED PATH (4%- orders): Corner cases延迟处理
// --------------------------------------------------------
// - 统一延迟队列 deferred_queue_: OrderId -> DeferredOrder
// - 四种延迟原因:
//   1) OUT_OF_ORDER: TAKER/CANCEL先于MAKER到达 (~2-5%概率)
//   2) CALL_AUCTION: 集合竞价期间MAKER价格虚假, 等待TAKER提供真实价格或9:30 flush
//   3) SPECIAL_MAKER: 特殊MAKER(市价单/本方最优)无价格, 等待TAKER提供成交价 (~1-2%概率)
//   4) ZERO_PRICE_CANCEL: 深交所撤单无价格, 等待MAKER提供价格 (~5-10%撤单概率)
//
// - MAKER处理流程:
//   1. [特殊条件检查] 集合竞价期? → 进queue (CALL_AUCTION)
//   2. [特殊条件检查] price=0? → 进queue (SPECIAL_MAKER)  
//   3. [Queue检查] queue中有对手单(TAKER/CANCEL)? → flush并抵扣 → 创建剩余量
//   4. [正常创建] 直接创建订单到LOB
//
// - TAKER处理流程 (统一抵扣模型: 不区分对手单类型):
//   1. [Queue检查] queue中有对手单? → 从queue消费对手单 (任何reason) → 结束
//   2. [LOB检查] LOB中有对手单? → 消费对手单 + 更新TOB
//   3. [乱序处理] 对手单未到达 → 自己进queue等待 (OUT_OF_ORDER)
//   4. [清理自身] 检查self_order_id是否在queue (特殊市价单清理)
//
// - CANCEL处理流程:
//   1. [Queue检查] queue中有自身订单? → 从queue抵扣 → 结束
//   2. [LOB检查] LOB中有自身订单? → 正常撤单
//   3. [乱序处理] 订单未到达 → 自己进queue等待 (OUT_OF_ORDER或ZERO_PRICE_CANCEL)
//
// - TOB跟踪 (Top of Book):
//   1. 不尝试在乱序期间维护精确TOB (交界处可能短暂混乱)
//   2. ask TOB = 最近买方TAKER成交的对手价格 (吃空则自动顺延)
//   3. bid TOB = 最近卖方TAKER成交的对手价格 (吃空则自动顺延)
//   4. 99%+时间TOB准确 (乱序窗口极短)
//
// - 集合竞价期间特殊处理:
//   1. 9:15-9:25: 集合竞价期, MAKER订单价格不一定是最终成交价(一部分统一竞价撮合价,一部分原始挂单价), 放入延迟队列
//   2. 9:25-9:30: 集合竞价撮合期, TAKER订单带来真实撮合价(统一竞价撮合价), 在延迟队列中找对手抵扣
//      - MAKER订单继续放入延迟队列(以便TAKER找到对手)
//      - TAKER订单在延迟队列中查找对应MAKER并抵扣
//   3. 9:30:00: 连续竞价开始, 将延迟队列中剩余的集合竞价订单按挂单价flush到LOB
//      - 这些订单是未被撮合价成交的订单, 按其原始挂单价进入LOB继续等待成交
//   4. 14:57-15:00: 收盘集合竞价(深圳), 处理逻辑相同

// 对于价格档位, 用位图 + 缓存向量的数据结构
// 对于订单, 用(内存连续)向量 + 哈希表的数据结构

//========================================================================================
// CONSTANTS AND TYPES
//========================================================================================

// LOB reconstruction engine configuration
static constexpr size_t EXPECTED_QUEUE_SIZE = 128;
static constexpr size_t CACHE_LINE_SIZE = 64;
static constexpr float HASH_LOAD_FACTOR = 0.4f; // Ultra-low load factor

// Core types
using Price = uint16_t;
using Quantity = int32_t; // Supports negative quantities for deduction model
using OrderId = uint32_t;

static constexpr uint32_t PRICE_RANGE_SIZE = static_cast<uint32_t>(UINT16_MAX) + 1; // 0-65535

// Ultra-compact order entry - cache-optimized
#if DEBUG_ANOMALY_PRINT
struct alignas(16) Order {
  Quantity qty;
  OrderId id;
  uint32_t timestamp; // Creation timestamp for debug only
  Order(Quantity q, OrderId i, uint32_t ts) : qty(q), id(i), timestamp(ts) {}
#else
struct alignas(8) Order {
  Quantity qty;
  OrderId id;
  Order(Quantity q, OrderId i) : qty(q), id(i) {}
#endif
  // Fast operations - always inline
  bool is_positive() const { return qty > 0; }
  bool is_depleted() const { return qty <= 0; }
  void subtract(Quantity amount) { qty -= amount; }
  void add(Quantity amount) { qty += amount; }
};

// Simple unified price level - no side field needed
struct alignas(CACHE_LINE_SIZE) Level {
  Price price;                    // Price level identifier
  Quantity net_quantity = 0;      // Cached sum of all quantities (can be negative)
  uint16_t order_count = 0;       // Fast size tracking
  uint16_t alignment_padding = 0; // Explicit padding for cache line alignment
  std::vector<Order *> orders;    // Pointers to orders at this price level

  explicit Level(Price p) : price(p) {
    orders.reserve(EXPECTED_QUEUE_SIZE);
  }

  // High-performance order management
  [[gnu::hot]] void add(Order *order) {
    orders.push_back(order);
    ++order_count;
    net_quantity += order->qty;
  }

  [[gnu::hot]] void remove(size_t order_index) {
    assert(order_index < orders.size());
    Order *removed_order = orders[order_index];

    // Update cached total before removal
    net_quantity -= removed_order->qty;

    // Swap-and-pop for O(1) removal
    if (order_index != orders.size() - 1) {
      orders[order_index] = orders.back();
    }
    orders.pop_back();
    --order_count;
  }

  // Fast level state queries
  bool empty() const { return order_count == 0; }
  bool has_visible_quantity() const { return net_quantity != 0; }

  // Recalculate cached total from scratch
  void refresh_total() {
    net_quantity = 0;
    for (const Order *current_order : orders) {
      net_quantity += current_order->qty;
    }
  }
};

// Order location tracking
struct Location {
  Level *level;
  size_t index;

  Location(Level *l, size_t i) : level(l), index(i) {}
};

// Deferred order reason enumeration
enum class DeferReason : uint8_t {
  OUT_OF_ORDER,      // TAKER/CANCEL arrived before MAKER (out-of-order)
  CALL_AUCTION,      // MAKER during call auction period (9:15-9:30), waiting for TAKER match or flush at 9:30
  SPECIAL_MAKER,     // MAKER with price=0 (market order '1', best-for-us 'U'), waiting for TAKER to provide price
  ZERO_PRICE_CANCEL  // CANCEL with price=0 (Shenzhen), waiting for MAKER's price
};

// Deferred order operation type for debugging
enum class DeferOp : uint8_t {
  CONSUME_BY_TAKER,      // TAKER consumed counterparty MAKER from queue (during matching period or out-of-order)
  CANCEL_SELF,           // CANCEL consumed self MAKER from queue
  FLUSH_BY_MAKER,        // MAKER arrived and flushed earlier TAKER/CANCEL from queue
  FLUSH_AT_CONTINUOUS,   // Flush remaining call auction orders to LOB at 9:30:00
  CLEANUP_SPECIAL        // Cleanup special market order after TAKER trade
};

// Deferred order information - compact design for corner cases
struct DeferredOrder {
  Quantity signed_volume;   // Signed quantity (positive=bid increase/ask decrease, negative=ask increase/bid decrease)
  Price reported_price;     // Reported price (fake for CALL_AUCTION MAKER, 0 for ZERO_PRICE)
  uint32_t timestamp;       // Creation timestamp (for debugging)
  DeferReason reason;       // Defer reason
  bool is_bid;              // Whether it's a bid order
  
  DeferredOrder(Quantity vol, Price price, uint32_t ts, DeferReason r, bool bid)
      : signed_volume(vol), reported_price(price), timestamp(ts), reason(r), is_bid(bid) {}
};

//========================================================================================
// MAIN CLASS
//========================================================================================

class AnalysisHighFrequency {

public:
  // ========================================================================================
  // CONSTRUCTOR
  // ========================================================================================

  explicit AnalysisHighFrequency(size_t ORDER_SIZE = L2::DEFAULT_ENCODER_ORDER_SIZE)
      : order_lookup_(&order_lookup_memory_pool_), order_memory_pool_(ORDER_SIZE) {
    // Configure hash table for minimal collisions based on custom order count
    const size_t HASH_BUCKETS = (1ULL << static_cast<size_t>(std::ceil(std::log2(ORDER_SIZE / HASH_LOAD_FACTOR))));
    order_lookup_.reserve(ORDER_SIZE);
    order_lookup_.rehash(HASH_BUCKETS);
    order_lookup_.max_load_factor(HASH_LOAD_FACTOR);
  }

  // ========================================================================================
  // PUBLIC INTERFACE - MAIN ENTRY POINTS
  // ========================================================================================

  // Main order processing entry point - with deferred queue for corner cases
  [[gnu::hot]] bool process(const L2::Order &order) {
    curr_tick_ = (order.hour << 24) | (order.minute << 16) | (order.second << 8) | order.millisecond;
    new_tick_ = curr_tick_ != prev_tick_;
    
    // Check for call auction/matching period transitions and handle deferred orders
    static bool was_in_matching_period = false;
    
    bool in_call_auction = is_call_auction_period();
    bool in_matching_period = is_call_auction_matching_period();
    
    // Entering continuous auction (9:30) - flush all remaining call auction orders to LOB
    if (was_in_matching_period && !in_matching_period && !in_call_auction) [[unlikely]] {
      // Exited matching period, entering continuous auction (9:30:00)
      // Flush remaining call auction orders to LOB at their reported price
      flush_call_auction_deferred();
    }
    
    was_in_matching_period = in_matching_period;
    
    print_book(); // print before updating prev_tick (such that current snapshot is a valid sample)
    prev_tick_ = curr_tick_;

    // Process resampling
    if (resampler_.process(order)) {
      // std::cout << "[RESAMPLE] Bar formed at " << format_time() << std::endl;
    }

    bool result = update_lob(order);
    return result;
  };

  [[gnu::hot, gnu::always_inline]] bool update_lob(const L2::Order &order) {
    // 1. Get signed volume and target ID using simple lookup functions
    signed_volume_ = get_signed_volume(order);
    target_id_ = get_target_id(order);
    if (signed_volume_ == 0 || target_id_ == 0) [[unlikely]]
      return false;
    
#if DEBUG_ANOMALY_PRINT
    debug_.last_order = order;
#endif

    // 2. Fast check: are we in call auction period? (cheap time check)
    const bool in_call_auction = is_call_auction_period();
    
    // ========================================================================
    // HOT PATH OPTIMIZATION: Most orders (98%+) follow the fast path below
    // We minimize branches and hash lookups on this path
    // ========================================================================
    
    // 3. Perform lookup for the incoming order (single hash lookup)
    auto order_lookup_iterator = order_lookup_.find(target_id_);
    const bool order_found = (order_lookup_iterator != order_lookup_.end());
    
    // ========================================================================
    // FAST PATH: TAKER/CANCEL with existing MAKER (most common case)
    // ========================================================================
    // Condition: queue is empty + order found in LOB
    if ((order.order_type == L2::OrderType::TAKER || order.order_type == L2::OrderType::CANCEL) && 
        order_found && deferred_queue_.empty()) [[likely]] {
      // HOT PATH: counterparty/self order exists in LOB, no queue to check
      effective_price_ = order_lookup_iterator->second.level->price;
      bool was_fully_consumed = apply_volume_change(target_id_, effective_price_, signed_volume_, order_lookup_iterator);
      
      if (order.order_type == L2::OrderType::TAKER) {
        update_tob_after_trade(order, was_fully_consumed, effective_price_);
      }
      return true;
    }
    
    // ========================================================================
    // FAST PATH: MAKER without deferred records (most common case)
    // ========================================================================
    if (order.order_type == L2::OrderType::MAKER && !in_call_auction && deferred_queue_.empty()) [[likely]] {
      // Check for special MAKER orders with price=0 (market orders, best-for-us orders)
      // These need to be deferred until matched by counterparty
      if (order.price == 0) [[unlikely]] {
        // Special MAKER order with no price - defer to queue
        return update_lob_deferred(order, order_lookup_iterator, order_found, in_call_auction);
      }
      
      // HOT PATH: normal MAKER in continuous auction, no deferred queue
      effective_price_ = order.price;
      apply_volume_change(target_id_, effective_price_, signed_volume_, order_lookup_iterator);
      return true;
    }
    
    // ========================================================================
    // SLOW PATH: Corner cases requiring deferred queue handling
    // ========================================================================
    return update_lob_deferred(order, order_lookup_iterator, order_found, in_call_auction);
  };
  
  // Deferred path for corner cases (out-of-order, call auction, zero-price)
  [[gnu::cold, gnu::noinline]] bool update_lob_deferred(
      const L2::Order &order,
      std::pmr::unordered_map<OrderId, Location>::iterator order_lookup_iterator,
      bool order_found,
      bool in_call_auction) {
    
    // Check if there's a deferred record for this order
    auto deferred_it = deferred_queue_.find(target_id_);
    const bool has_deferred = (deferred_it != deferred_queue_.end());
    
    // ========================================================================
    // MAKER ORDER - Deferred Path
    // ========================================================================
    if (order.order_type == L2::OrderType::MAKER) {
      // Check if we're in call auction or matching period (9:15-9:30)
      // During both periods, MAKER orders should go to deferred queue
      const bool in_call_auction_extended = in_call_auction || is_call_auction_matching_period();
      
      // CORNER CASE: MAKER with price=0 (special orders: market order, best-for-us)
      // These orders need to wait in queue for TAKER to provide the matching price
      if (order.price == 0) [[unlikely]] {
        if (has_deferred) {
          // Existing deferred record (likely earlier TAKER/CANCEL) - flush it
          signed_volume_ += deferred_it->second.signed_volume;
          const Quantity final_volume = (signed_volume_ == 0) ? 0 : signed_volume_;
#if DEBUG_DEFERRED_FLUSH
          print_deferred_dequeue(deferred_it->second, target_id_, final_volume, DeferOp::FLUSH_BY_MAKER);
#endif
          deferred_queue_.erase(deferred_it);
          
          if (signed_volume_ == 0) {
            return true; // Fully offset, no order created
          }
        }
        
        // Put zero-price MAKER into deferred queue, waiting for TAKER to match
        DeferredOrder deferred_order(signed_volume_, 0, curr_tick_, 
                                     DeferReason::SPECIAL_MAKER, 
                                     order.order_dir == L2::OrderDirection::BID);
        deferred_queue_.emplace(target_id_, deferred_order);
#if DEBUG_DEFERRED_ENQUEUE
        print_deferred_enqueue(order, deferred_order);
#endif
        return true;
      }
      
      if (in_call_auction_extended) {
        // CORNER CASE: Call auction/matching period - MAKER may need to wait for TAKER
        // 9:15-9:25: MAKER price may be fake, defer until TAKER arrives
        // 9:25-9:30: MAKER should also go to queue so TAKER can find and match them
        if (has_deferred) {
          // Existing deferred record (likely earlier TAKER/CANCEL) - flush it
          signed_volume_ += deferred_it->second.signed_volume;
          const Quantity final_volume = (signed_volume_ == 0) ? 0 : signed_volume_;
#if DEBUG_DEFERRED_FLUSH
          print_deferred_dequeue(deferred_it->second, target_id_, final_volume, DeferOp::FLUSH_BY_MAKER);
#endif
          deferred_queue_.erase(deferred_it);
          
          if (signed_volume_ == 0) {
            return true; // Fully offset, no order created
          }
        }
        
        // Put MAKER into deferred queue, waiting for TAKER to match or flush at 9:30
        DeferredOrder deferred_order(signed_volume_, order.price, curr_tick_, 
                                     DeferReason::CALL_AUCTION, 
                                     order.order_dir == L2::OrderDirection::BID);
        deferred_queue_.emplace(target_id_, deferred_order);
#if DEBUG_DEFERRED_ENQUEUE
        print_deferred_enqueue(order, deferred_order);
#endif
        return true;
      }
      
      // CORNER CASE: Continuous auction with deferred records - flush them
      if (has_deferred) {
        signed_volume_ += deferred_it->second.signed_volume;
        const Quantity final_volume = (signed_volume_ == 0) ? 0 : signed_volume_;
#if DEBUG_DEFERRED_FLUSH
        print_deferred_dequeue(deferred_it->second, target_id_, final_volume, DeferOp::FLUSH_BY_MAKER);
#endif
        deferred_queue_.erase(deferred_it);
        
        if (signed_volume_ == 0) {
          return true; // Fully offset, no order created
        }
      }
      
      // Create MAKER order to LOB
      effective_price_ = order.price;
      apply_volume_change(target_id_, effective_price_, signed_volume_, order_lookup_iterator);
      return true;
    }
    
    // ========================================================================
    // TAKER ORDER - Deferred Path (Queue优先策略)
    // ========================================================================
    if (order.order_type == L2::OrderType::TAKER) {
      // Step 1: Check deferred_queue for counterparty (unified consumption, any reason)
      if (has_deferred) {
        // CORNER CASE: Counterparty MAKER in deferred queue - consume it
        // Reasons: CALL_AUCTION (集合竞价), SPECIAL_MAKER (市价单), or future extensions
        // We don't distinguish reasons - unified deduction model
        
        Quantity maker_volume = deferred_it->second.signed_volume;
        Quantity net_volume = maker_volume + signed_volume_;
        
        // Check if MAKER is fully consumed or over-consumed (sign reversal)
        const bool fully_consumed = (net_volume == 0) || 
                                     (maker_volume > 0 && net_volume <= 0) || 
                                     (maker_volume < 0 && net_volume >= 0);
        
        if (fully_consumed) {
          // MAKER fully consumed - remove from queue
#if DEBUG_DEFERRED_FLUSH
          print_deferred_dequeue(deferred_it->second, target_id_, 0, DeferOp::CONSUME_BY_TAKER);
#endif
          deferred_queue_.erase(deferred_it);
        } else {
          // MAKER partially consumed - update its volume in queue
#if DEBUG_DEFERRED_FLUSH
          print_deferred_dequeue(deferred_it->second, target_id_, net_volume, DeferOp::CONSUME_BY_TAKER);
#endif
          deferred_it->second.signed_volume = net_volume;
        }
        
        // Trade completed, now check and clean up self order if exists
        // (Special market order cleanup: market orders enter as MAKER, then trade as TAKER)
        const bool is_bid = (order.order_dir == L2::OrderDirection::BID);
        const OrderId self_order_id = is_bid ? order.bid_order_id : order.ask_order_id;
        
        if (self_order_id != 0 && self_order_id != target_id_) [[unlikely]] {
          auto self_deferred_it = deferred_queue_.find(self_order_id);
          if (self_deferred_it != deferred_queue_.end() && 
              self_deferred_it->second.reason == DeferReason::SPECIAL_MAKER) [[unlikely]] {
#if DEBUG_DEFERRED_FLUSH
            print_deferred_dequeue(self_deferred_it->second, self_order_id, 0, DeferOp::CLEANUP_SPECIAL);
#endif
            deferred_queue_.erase(self_deferred_it);
          }
        }
        
        return true; // Trade completed
      }
      
      // Step 2: Check LOB for counterparty (normal case when queue is not empty but target not in queue)
      if (order_found) {
        // Counterparty exists in LOB - consume it
        effective_price_ = order_lookup_iterator->second.level->price;
        bool was_fully_consumed = apply_volume_change(target_id_, effective_price_, signed_volume_, order_lookup_iterator);
        update_tob_after_trade(order, was_fully_consumed, effective_price_);
        
        // Check and clean up self order if exists (special market order)
        const bool is_bid = (order.order_dir == L2::OrderDirection::BID);
        const OrderId self_order_id = is_bid ? order.bid_order_id : order.ask_order_id;
        
        if (self_order_id != 0 && self_order_id != target_id_) [[unlikely]] {
          auto self_deferred_it = deferred_queue_.find(self_order_id);
          if (self_deferred_it != deferred_queue_.end() && 
              self_deferred_it->second.reason == DeferReason::SPECIAL_MAKER) [[unlikely]] {
#if DEBUG_DEFERRED_FLUSH
            print_deferred_dequeue(self_deferred_it->second, self_order_id, 0, DeferOp::CLEANUP_SPECIAL);
#endif
            deferred_queue_.erase(self_deferred_it);
          }
        }
        
        return true;
      }
      
      // Step 3: Counterparty not found anywhere - out-of-order case
      // CORNER CASE: TAKER arrived before counterparty MAKER
      DeferredOrder deferred_order(signed_volume_, order.price, curr_tick_,
                                   DeferReason::OUT_OF_ORDER,
                                   order.order_dir == L2::OrderDirection::BID);
      deferred_queue_.emplace(target_id_, deferred_order);
#if DEBUG_DEFERRED_ENQUEUE
      print_deferred_enqueue(order, deferred_order);
#endif
      return true;
    }
    
    // ========================================================================
    // CANCEL ORDER - Deferred Path (Queue优先策略)
    // ========================================================================
    if (order.order_type == L2::OrderType::CANCEL) {
      // Step 1: Check deferred_queue for self order (any reason)
      if (has_deferred) {
        // CORNER CASE: Self MAKER in deferred queue - cancel from queue
        // Reasons: CALL_AUCTION (集合竞价), SPECIAL_MAKER (市价单), or future extensions
        // Multiple CANCELs may partially cancel the same MAKER gradually
        
        Quantity maker_volume = deferred_it->second.signed_volume;
        Quantity net_volume = maker_volume + signed_volume_;
        
        // Check if MAKER is fully cancelled or over-cancelled (sign reversal)
        const bool fully_cancelled = (net_volume == 0) || 
                                      (maker_volume > 0 && net_volume <= 0) || 
                                      (maker_volume < 0 && net_volume >= 0);
        
        if (fully_cancelled) {
          // Fully cancelled or over-cancelled - remove from queue
#if DEBUG_DEFERRED_FLUSH
          print_deferred_dequeue(deferred_it->second, target_id_, 0, DeferOp::CANCEL_SELF);
#endif
          deferred_queue_.erase(deferred_it);
        } else {
          // Partial cancellation - update deferred quantity in queue
#if DEBUG_DEFERRED_FLUSH
          print_deferred_dequeue(deferred_it->second, target_id_, net_volume, DeferOp::CANCEL_SELF);
#endif
          deferred_it->second.signed_volume = net_volume;
        }
        return true;
      }
      
      // Step 2: Check LOB for self order (normal case when queue is not empty but target not in queue)
      if (order_found) {
        // Self order exists in LOB - normal cancellation
        effective_price_ = order_lookup_iterator->second.level->price;
        apply_volume_change(target_id_, effective_price_, signed_volume_, order_lookup_iterator);
        return true;
      }
      
      // Step 3: Self order not found anywhere - out-of-order or zero-price case
      // CORNER CASE: CANCEL arrived before self MAKER
      DeferredOrder deferred_order(signed_volume_, order.price, curr_tick_,
                                   order.price == 0 ? DeferReason::ZERO_PRICE_CANCEL : DeferReason::OUT_OF_ORDER,
                                   order.order_dir == L2::OrderDirection::BID);
      deferred_queue_.emplace(target_id_, deferred_order);
#if DEBUG_DEFERRED_ENQUEUE
      print_deferred_enqueue(order, deferred_order);
#endif
      return true;
    }
    
    return false;
  };

  // ========================================================================================
  // PUBLIC INTERFACE - DATA ACCESS
  // ========================================================================================

  // Get best bid price
  [[gnu::hot]] Price best_bid() const {
    update_tob();
    return best_bid_;
  }

  // Get best ask price
  [[gnu::hot]] Price best_ask() const {
    update_tob();
    return best_ask_;
  }

  // Book statistics - optimized for performance
  size_t total_orders() const { return order_lookup_.size(); }
  size_t total_levels() const { return price_levels_.size(); }
  size_t total_deferred() const { return deferred_queue_.size(); }  // Number of deferred orders in corner case queue
  
  // Detailed deferred statistics (for debugging)
  size_t total_deferred_by_reason(DeferReason reason) const {
    size_t count = 0;
    for (const auto& [id, order] : deferred_queue_) {
      if (order.reason == reason) ++count;
    }
    return count;
  }

  // ========================================================================================
  // PUBLIC INTERFACE - BATCH PROCESSING
  // ========================================================================================

  // Simple batch processing
  template <typename OrderRange>
  [[gnu::hot]] size_t process_batch(const OrderRange &order_range) {
    size_t successfully_processed = 0;

    for (const auto &current_order : order_range) {
      if (process(current_order))
        ++successfully_processed;
    }

    return successfully_processed;
  }

  // ========================================================================================
  // PUBLIC INTERFACE - MARKET DEPTH ITERATION
  // ========================================================================================

  // Iterate through bid levels (price >= best_bid_) - optimized with cache
  template <typename Func>
  void for_each_visible_bid(Func &&callback_function, size_t max_levels = 5) const {
    update_tob();
    refresh_cache_if_dirty();

    if (best_bid_ == 0 || cached_visible_prices_.empty())
      return;

    // Find position of best_bid_ in sorted cache
    auto it = std::upper_bound(cached_visible_prices_.begin(), cached_visible_prices_.end(), best_bid_);

    size_t levels_processed = 0;
    // Iterate backwards from best_bid position for descending price order
    while (it != cached_visible_prices_.begin() && levels_processed < max_levels) {
      --it;
      Price price = *it;
      Level *level = find_level(price);
      if (level && level->has_visible_quantity()) {
        callback_function(price, level->net_quantity);
        ++levels_processed;
      }
    }
  }

  // Iterate through ask levels (price <= best_ask_) - optimized with cache
  template <typename Func>
  void for_each_visible_ask(Func &&callback_function, size_t max_levels = 5) const {
    update_tob();
    refresh_cache_if_dirty();

    if (best_ask_ == 0 || cached_visible_prices_.empty())
      return;

    // Find position of best_ask_ in sorted cache
    auto it = std::lower_bound(cached_visible_prices_.begin(), cached_visible_prices_.end(), best_ask_);

    size_t levels_processed = 0;
    // Iterate forward from best_ask position for ascending price order
    for (; it != cached_visible_prices_.end() && levels_processed < max_levels; ++it) {
      Price price = *it;
      Level *level = find_level(price);
      if (level && level->has_visible_quantity()) {
        callback_function(price, level->net_quantity);
        ++levels_processed;
      }
    }
  }

  // ========================================================================================
  // PUBLIC INTERFACE - UTILITIES
  // ========================================================================================

  // Complete reset
  void clear() {
    price_levels_.clear();
    level_storage_.clear();
    order_lookup_.clear();
    order_memory_pool_.reset();
    deferred_queue_.clear();  // Clear unified deferred queue
    visible_price_bitmap_.reset(); // O(1) clear all bits
    cached_visible_prices_.clear();
    cache_dirty_ = false;
    best_bid_ = 0;
    best_ask_ = 0;
    tob_dirty_ = true;
    prev_tick_ = 0;
    curr_tick_ = 0;
    new_tick_ = false;
    signed_volume_ = 0;
    target_id_ = 0;
    effective_price_ = 0;
#if DEBUG_ANOMALY_PRINT
    debug_.printed_anomalies.clear();
#endif

    if (DEBUG_SINGLE_DAY) {
      exit(1);
    }
  }

private:
  // ========================================================================================
  // CORE DATA STRUCTURES
  // ========================================================================================

  // Price level storage (stable memory addresses via deque)
  std::deque<Level> level_storage_;                 // All price levels (deque guarantees stable pointers)
  std::unordered_map<Price, Level *> price_levels_; // Price -> Level* mapping for O(1) lookup

  // Visible price tracking (prices with non-zero net_quantity)
  std::bitset<PRICE_RANGE_SIZE> visible_price_bitmap_; // Bitmap for O(1) visibility check
  mutable std::vector<Price> cached_visible_prices_;   // Sorted cache for fast iteration
  mutable bool cache_dirty_ = false;                   // Cache needs refresh flag

  // Top of book tracking
  mutable Price best_bid_ = 0;   // Best bid price (highest buy price with visible quantity)
  mutable Price best_ask_ = 0;   // Best ask price (lowest sell price with visible quantity)
  mutable bool tob_dirty_ = true; // TOB needs recalculation flag

  // Order tracking infrastructure
  std::pmr::unsynchronized_pool_resource order_lookup_memory_pool_;  // PMR memory pool for hash map
  std::pmr::unordered_map<OrderId, Location> order_lookup_;          // OrderId -> Location(Level*, index) for O(1) order lookup
  MemPool::MemoryPool<Order> order_memory_pool_;                     // Memory pool for Order object allocation

  // Unified deferred queue for corner cases (typically <100 entries, ~0.1% of total orders)
  // Four defer reasons:
  // 1. OUT_OF_ORDER: TAKER/CANCEL arrived before MAKER (乱序)
  // 2. CALL_AUCTION: MAKER during call auction period (9:15-9:30), waiting for TAKER match or 9:30 flush (集合竞价)
  // 3. SPECIAL_MAKER: MAKER with price=0 (market order/best-for-us), waiting for TAKER to provide price (特殊挂单)
  // 4. ZERO_PRICE_CANCEL: CANCEL with price=0 (Shenzhen), waiting for MAKER's price (零价格撤单)
  // Queue is flushed when:
  //   - Corresponding MAKER/TAKER arrives (for OUT_OF_ORDER and matching in 9:25-9:30)
  //   - Entering continuous auction at 9:30 (remaining CALL_AUCTION orders flushed to LOB at reported price)
  std::unordered_map<OrderId, DeferredOrder> deferred_queue_;  // OrderId -> DeferredOrder

  // Market timestamp tracking (hour|minute|second|millisecond)
  uint32_t prev_tick_ = 0;  // Previous tick timestamp
  uint32_t curr_tick_ = 0;  // Current tick timestamp
  bool new_tick_ = false;   // Flag: entered new tick

  // Hot path temporary variable cache to reduce allocation overhead
  mutable Quantity signed_volume_;
  mutable OrderId target_id_;
  mutable Price effective_price_;

  // Resampling components
  ResampleRunBar resampler_;

  // ========================================================================================
  // CORE LOB MANAGEMENT - LEVEL OPERATIONS
  // ========================================================================================

  // Simple price level lookup
  [[gnu::hot, gnu::always_inline]] inline Level *find_level(Price price) const {
    auto level_iterator = price_levels_.find(price);
    return (level_iterator != price_levels_.end()) ? level_iterator->second : nullptr;
  }

  // Create new price level
  [[gnu::hot, gnu::always_inline]] Level *create_level(Price price) {
    level_storage_.emplace_back(price);
    Level *new_level = &level_storage_.back();
    price_levels_[price] = new_level;
    return new_level;
  }

  // Remove empty level
  [[gnu::hot, gnu::always_inline]] void remove_level(Level *level_to_remove, bool erase_visible = true) {
    price_levels_.erase(level_to_remove->price);
    if (erase_visible) {
      remove_visible_price(level_to_remove->price);
    }
  }

  // ========================================================================================
  // CORE LOB MANAGEMENT - VISIBLE PRICE TRACKING
  // ========================================================================================

  // High-performance visible price cache management
  [[gnu::hot, gnu::always_inline]] inline void refresh_cache_if_dirty() const {
    if (!cache_dirty_)
      return;

    cached_visible_prices_.clear();
    for (uint32_t price_u32 = 0; price_u32 < PRICE_RANGE_SIZE; ++price_u32) {
      Price price = static_cast<Price>(price_u32);
      if (visible_price_bitmap_[price]) {
        cached_visible_prices_.push_back(price);
      }
    }
    cache_dirty_ = false;
  }

  // O(1) visible price addition
  [[gnu::hot, gnu::always_inline]] inline void add_visible_price(Price price) {
    if (!visible_price_bitmap_[price]) {
      visible_price_bitmap_.set(price);
      cache_dirty_ = true;
    }
  }

  // O(1) visible price removal
  [[gnu::hot, gnu::always_inline]] inline void remove_visible_price(Price price) {
    if (visible_price_bitmap_[price]) {
      visible_price_bitmap_.reset(price);
      cache_dirty_ = true;
    }
  }

  // Maintain visible price ordering after any level total change - now O(1)!
  [[gnu::hot, gnu::always_inline]] inline void update_visible_price(Level *level) {
    if (level->has_visible_quantity()) {
      add_visible_price(level->price);
    } else {
      remove_visible_price(level->price);
    }
  }

  // Find next ask level strictly above a given price with visible quantity - O(n) worst case, but typically very fast
  Price next_ask_above(Price from_price) const {
    for (uint32_t price_u32 = static_cast<uint32_t>(from_price) + 1; price_u32 < PRICE_RANGE_SIZE; ++price_u32) {
      Price price = static_cast<Price>(price_u32);
      if (visible_price_bitmap_[price]) {
        return price;
      }
    }
    return 0;
  }

  // Find next bid level strictly below a given price with visible quantity - O(n) worst case, but typically very fast
  Price next_bid_below(Price from_price) const {
    if (from_price == 0)
      return 0;
    for (Price price = from_price - 1; price != UINT16_MAX; --price) {
      if (visible_price_bitmap_[price]) {
        return price;
      }
    }
    return 0;
  }

  // Find minimum price with visible quantity - O(n) worst case, but cache-friendly
  Price min_visible_price() const {
    refresh_cache_if_dirty();
    return cached_visible_prices_.empty() ? 0 : cached_visible_prices_.front();
  }

  // Find maximum price with visible quantity - O(n) worst case, but cache-friendly
  Price max_visible_price() const {
    refresh_cache_if_dirty();
    return cached_visible_prices_.empty() ? 0 : cached_visible_prices_.back();
  }

  // ========================================================================================
  // CORE LOB MANAGEMENT - ORDER PROCESSING
  // ========================================================================================

  // Simplified unified volume calculation
  [[gnu::hot, gnu::always_inline]] inline Quantity get_signed_volume(const L2::Order &order) const {
    const bool is_bid = (order.order_dir == L2::OrderDirection::BID);

    switch (order.order_type) {
    case L2::OrderType::MAKER:
      return is_bid ? +order.volume : -order.volume;
    case L2::OrderType::CANCEL:
      return is_bid ? -order.volume : +order.volume;
    case L2::OrderType::TAKER:
      return is_bid ? +order.volume : -order.volume;
    default:
      return 0;
    }
  }

  // Simplified unified target ID lookup
  [[gnu::hot, gnu::always_inline]] inline OrderId get_target_id(const L2::Order &order) const {
    const bool is_bid = (order.order_dir == L2::OrderDirection::BID);

    switch (order.order_type) {
    case L2::OrderType::MAKER:
      return is_bid ? order.bid_order_id : order.ask_order_id;
    case L2::OrderType::CANCEL:
      return is_bid ? order.bid_order_id : order.ask_order_id;
    case L2::OrderType::TAKER:
      return is_bid ? order.ask_order_id : order.bid_order_id;
    default:
      return 0;
    }
  }

  // Unified order processing core logic - now accepts lookup iterator from caller
  [[gnu::hot, gnu::always_inline]] bool apply_volume_change(
      OrderId target_id,
      Price price,
      Quantity signed_volume,
      decltype(order_lookup_.find(target_id)) order_lookup_iterator) {

    if (order_lookup_iterator != order_lookup_.end()) {
      // Order exists - modify it
      Level *target_level = order_lookup_iterator->second.level;
      size_t order_index = order_lookup_iterator->second.index;
      Order *target_order = target_level->orders[order_index];

      // Apply signed volume change
      const Quantity old_qty = target_order->qty;
      const Quantity new_qty = old_qty + signed_volume;

      if (new_qty == 0) {
        // std::cout << "Order fully consumed: " << target_id << " at price: " << price << " with volume: " << signed_volume << std::endl;

        // Order fully consumed - remove completely
        target_level->remove(order_index);
        order_lookup_.erase(order_lookup_iterator);

        // Update lookup index for any moved order (swap-and-pop side effect)
        if (order_index < target_level->orders.size()) {
          auto moved_order_lookup = order_lookup_.find(target_level->orders[order_index]->id);
          if (moved_order_lookup != order_lookup_.end()) {
            moved_order_lookup->second.index = order_index;
          }
        }

        // Handle level cleanup and TOB updates
        if (target_level->empty()) {
          remove_level(target_level);
        } else {
          update_visible_price(target_level);
        }

        return true; // Fully consumed
      } else {
        // Partial update
        target_level->net_quantity += signed_volume;
        target_order->qty = new_qty;
        update_visible_price(target_level);
        return false; // Partially consumed
      }

    } else {
      // Order doesn't exist - create placeholder
#if DEBUG_ANOMALY_PRINT
      Order *new_order = order_memory_pool_.construct(signed_volume, target_id, curr_tick_);
#else
      Order *new_order = order_memory_pool_.construct(signed_volume, target_id);
#endif
      if (!new_order)
        return false;

      Level *target_level = find_level(price);
      if (!target_level) {
        target_level = create_level(price);
      }

      size_t new_order_index = target_level->orders.size();
      target_level->add(new_order);
      order_lookup_.emplace(target_id, Location(target_level, new_order_index));
      update_visible_price(target_level);
      return false; // New order created
    }
  }

  // ========================================================================================
  // CORE LOB MANAGEMENT - TOP OF BOOK (TOB)
  // ========================================================================================

  // Simple TOB update when needed (bootstrap only)
  void update_tob() const {
    if (!tob_dirty_)
      return;

    if (best_bid_ == 0 && best_ask_ == 0) {
      best_bid_ = max_visible_price();
      best_ask_ = min_visible_price();
    }

    tob_dirty_ = false;
  }

  // TOB update logic for taker orders
  [[gnu::hot, gnu::always_inline]] inline void update_tob_after_trade(const L2::Order &order, bool was_fully_consumed, Price trade_price) {
    const bool is_bid = (order.order_dir == L2::OrderDirection::BID);

    if (was_fully_consumed) {
      // Level was emptied - advance TOB
      if (is_bid) {
        // Buy taker consumed ask - advance ask to higher price
        best_ask_ = next_ask_above(trade_price);
      } else {
        // Sell taker consumed bid - advance bid to lower price
        best_bid_ = next_bid_below(trade_price);
      }
    } else {
      // Partial fill - TOB stays at this price
      if (is_bid) {
        best_ask_ = trade_price;
      } else {
        best_bid_ = trade_price;
      }
    }

    tob_dirty_ = false;
  }

  // ========================================================================================
  // HELPER UTILITIES - CALL AUCTION HANDLING
  // ========================================================================================
  
  // Check if current time is in call auction period (order collection phase)
  // Call auction periods: 9:15-9:25 (open), 14:57-15:00 (close, Shenzhen)
  [[gnu::hot, gnu::always_inline]] inline bool is_call_auction_period() const {
    using namespace TradingSession;
    const uint8_t hour = (curr_tick_ >> 24) & 0xFF;
    const uint8_t minute = (curr_tick_ >> 16) & 0xFF;
    
    // Morning call auction: 9:15-9:25 (not including 9:25, actual matching at 9:25:00)
    if (hour == MORNING_CALL_AUCTION_START_HOUR && 
        minute >= MORNING_CALL_AUCTION_START_MINUTE && 
        minute < MORNING_CALL_AUCTION_END_MINUTE) return true;
    
    // Closing call auction: 14:57-15:00 (Shenzhen only)
    // Note: Shanghai doesn't have closing call auction
    // TODO: Add exchange type check if needed
    if (hour == CLOSING_CALL_AUCTION_START_HOUR && minute >= CLOSING_CALL_AUCTION_START_MINUTE) return true;
    if (hour == CLOSING_CALL_AUCTION_END_HOUR && minute == CLOSING_CALL_AUCTION_END_MINUTE) return true;
    
    return false;
  }
  
  // Check if current time is in call auction matching period (9:25-9:30)
  // During this period, TAKER orders arrive with real matching price
  [[gnu::hot, gnu::always_inline]] inline bool is_call_auction_matching_period() const {
    using namespace TradingSession;
    const uint8_t hour = (curr_tick_ >> 24) & 0xFF;
    const uint8_t minute = (curr_tick_ >> 16) & 0xFF;
    
    // Morning matching period: 9:25-9:30
    if (hour == MORNING_CALL_AUCTION_START_HOUR && 
        minute >= MORNING_MATCHING_START_MINUTE && 
        minute < MORNING_MATCHING_END_MINUTE) return true;
    
    return false;
  }
  
  // Flush deferred call auction orders to LOB at their reported price (9:30:00)
  // At 9:30, all remaining call auction orders in queue should be flushed to level
  // using their original reported price (挂单价), not discarded
  void flush_call_auction_deferred() {
    auto it = deferred_queue_.begin();
    while (it != deferred_queue_.end()) {
      if (it->second.reason == DeferReason::CALL_AUCTION) {
        const OrderId order_id = it->first;
        const DeferredOrder& deferred = it->second;
        
        // Use the original reported price (挂单价) to flush to level
        const Price flush_price = deferred.reported_price;
        const Quantity flush_volume = deferred.signed_volume;
        
#if DEBUG_DEFERRED_FLUSH
        print_deferred_dequeue(deferred, order_id, 0, DeferOp::FLUSH_AT_CONTINUOUS);
#endif
        
        // Erase from deferred queue first
        it = deferred_queue_.erase(it);
        
        // Flush to LOB at reported price (挂单价)
        // Check if order already exists in order_lookup_ (unlikely but possible)
        auto lookup_it = order_lookup_.find(order_id);
        apply_volume_change(order_id, flush_price, flush_volume, lookup_it);
      } else {
        ++it;
      }
    }
  }
  
  // ========================================================================================
  // DEBUG PRINT FUNCTIONS - UNIFIED INTERFACE
  // ========================================================================================
  
  // Helper: Get reason string for deferred order
  static const char* get_defer_reason_str(DeferReason reason) {
    switch (reason) {
      case DeferReason::OUT_OF_ORDER:      return "OUT_OF_ORDER    ";
      case DeferReason::CALL_AUCTION:      return "CALL_AUCTION    ";
      case DeferReason::SPECIAL_MAKER:     return "SPECIAL_MAKER   ";
      case DeferReason::ZERO_PRICE_CANCEL: return "ZERO_PRICE_CNCL";
      default:                             return "UNKNOWN         ";
    }
  }
  
  // Helper: Get operation type string for deferred order dequeue
  static const char* get_defer_op_str(DeferOp op) {
    switch (op) {
      case DeferOp::CONSUME_BY_TAKER:    return "CONSUME_BY_TAKER";
      case DeferOp::CANCEL_SELF:         return "CANCEL_SELF     ";
      case DeferOp::FLUSH_BY_MAKER:      return "FLUSH_BY_MAKER  ";
      case DeferOp::FLUSH_AT_CONTINUOUS: return "FLUSH_AT_930    ";
      case DeferOp::CLEANUP_SPECIAL:     return "CLEANUP_SPECIAL ";
      default:                           return "UNKNOWN_OP      ";
    }
  }
  
#if DEBUG_DEFERRED_ENQUEUE
  // 🟡 ENQUEUE: Print when order enters deferred_queue_ (Yellow)
  void print_deferred_enqueue(const L2::Order &order, const DeferredOrder &deferred) const {
    char type_char = (order.order_type == L2::OrderType::MAKER) ? 'M' : 
                     (order.order_type == L2::OrderType::TAKER) ? 'T' : 'C';
    
    std::cout << "\033[33m[DEFER_ENQ] " << format_time() 
              << " | " << get_defer_reason_str(deferred.reason)
              << " | Type=" << type_char
              << " Dir=" << (deferred.is_bid ? 'B' : 'S')
              << " ID=" << std::setw(7) << std::right << target_id_
              << " Price=" << std::setw(5) << std::right << deferred.reported_price
              << " SignedVol=" << std::setw(6) << std::right << deferred.signed_volume
              << " | QueueSize=" << std::setw(3) << std::right << (deferred_queue_.size() + 1)
              << "\033[0m\n";
  }
#endif
  
#if DEBUG_DEFERRED_FLUSH
  // 🔵 DEQUEUE: Print when order is consumed/removed from deferred_queue_ (Blue)
  // Unified function for all queue operations: consume, cancel, flush, cleanup
  // - final_volume == 0: completely removed (erase)
  // - final_volume != 0: partially consumed (update, stays in queue)
  void print_deferred_dequeue(const DeferredOrder &deferred, OrderId order_id, Quantity final_volume, DeferOp op) const {
    const char* action = (final_volume == 0) ? "ERASE " : "REDUCE";
    
    std::cout << "\033[36m[DEFER_" << action << "] " << format_time() 
              << " | " << get_defer_op_str(op)
              << " | " << get_defer_reason_str(deferred.reason)
              << " | Dir=" << (deferred.is_bid ? 'B' : 'S')
              << " ID=" << std::setw(7) << std::right << order_id
              << " Vol=" << std::setw(6) << std::right << deferred.signed_volume
              << " → " << std::setw(6) << std::right << final_volume
              << " | QueueSize=" << std::setw(3) << std::right << (deferred_queue_.size() + (final_volume != 0 ? 0 : -1))
              << "\033[0m\n";
  }
#endif

  // ========================================================================================
  // HELPER UTILITIES - TIME FORMATTING
  // ========================================================================================

  // Convert packed timestamp to human-readable format
  std::string format_time() const {
    uint8_t hours = (curr_tick_ >> 24) & 0xFF;
    uint8_t minutes = (curr_tick_ >> 16) & 0xFF;
    uint8_t seconds = (curr_tick_ >> 8) & 0xFF;
    uint8_t milliseconds = curr_tick_ & 0xFF;

    std::ostringstream time_formatter;
    time_formatter << std::setfill('0')
                   << std::setw(2) << int(hours) << ":"
                   << std::setw(2) << int(minutes) << ":"
                   << std::setw(2) << int(seconds) << "."
                   << std::setw(3) << int(milliseconds * 10);
    return time_formatter.str();
  }

  // ========================================================================================
  // DEBUG INFRASTRUCTURE AND FUNCTIONS
  // ========================================================================================
#if DEBUG_ANOMALY_PRINT
  
  // Debug state storage
  struct DebugState {
    L2::Order last_order;
    std::unordered_set<Price> printed_anomalies;
  };
  mutable DebugState debug_;

  // Format timestamp as HH:MM:SS.mmm
  inline std::string format_timestamp(uint32_t ts) const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << ((ts >> 24) & 0xFF) << ":"
        << std::setw(2) << ((ts >> 16) & 0xFF) << ":"
        << std::setw(2) << ((ts >> 8) & 0xFF) << "."
        << std::setw(3) << ((ts & 0xFF) * 10);
    return oss.str();
  }

  // Calculate order age in milliseconds
  inline int calc_age_ms(uint32_t order_ts) const {
    constexpr auto to_ms = [](uint32_t ts) constexpr -> int {
      return ((ts >> 24) & 0xFF) * 3600000 + ((ts >> 16) & 0xFF) * 60000 +
             ((ts >> 8) & 0xFF) * 1000 + (ts & 0xFF) * 10;
    };
    return to_ms(curr_tick_) - to_ms(order_ts);
  }

  // Check for sign anomaly in level (print far anomalies N+ ticks from TOB during continuous trading)
  void check_anomaly(Level *level) const {
    using namespace TradingSession;
    using namespace AnomalyDetection;
    update_tob();
    
    // Step 1: Distance filter - only check far levels (N+ ticks from TOB)
    const bool is_far_below_bid = (best_bid_ > 0 && level->price < best_bid_ - MIN_DISTANCE_FROM_TOB);
    const bool is_far_above_ask = (best_ask_ > 0 && level->price > best_ask_ + MIN_DISTANCE_FROM_TOB);
    if (!is_far_below_bid && !is_far_above_ask) return;
    
    // Step 2: Classify by price relative to TOB mid price
    const Price tob_mid = (best_bid_ + best_ask_) / 2;
    const bool is_bid_side = (level->price < tob_mid);
    
    const bool has_anomaly = (is_bid_side && level->net_quantity < 0) || (!is_bid_side && level->net_quantity > 0);
    
    // Skip if no anomaly or already printed
    if (!has_anomaly) return;
    if (debug_.printed_anomalies.count(level->price)) return;
    
    // Step 3: Time filter - only print during continuous trading (09:30-15:00)
    const uint8_t hour = (curr_tick_ >> 24) & 0xFF;
    const uint8_t minute = (curr_tick_ >> 16) & 0xFF;
    if (!((hour == CONTINUOUS_TRADING_START_HOUR && minute >= CONTINUOUS_TRADING_START_MINUTE) || 
          (hour >= 10 && hour < CONTINUOUS_TRADING_END_HOUR))) {
      return; // Anomaly exists but not printed (call auction period)
    }
    
    // Print and mark as printed
    debug_.printed_anomalies.insert(level->price);
    print_anomaly_level(level, is_bid_side);
  }
  
  // Print detailed anomaly information for a level
  void print_anomaly_level(Level *level, bool is_bid_side) const {
    // Collect all reverse-sign orders (unmatched orders)
    std::vector<Order *> anomaly_orders;
    anomaly_orders.reserve(level->order_count); // Pre-allocate
    
    for (Order *order : level->orders) {
      const bool is_reverse = (is_bid_side && order->qty < 0) || (!is_bid_side && order->qty > 0);
      if (is_reverse) anomaly_orders.push_back(order);
    }
    if (anomaly_orders.empty()) return;
    
    // Sort by absolute size (largest first)
    std::sort(anomaly_orders.begin(), anomaly_orders.end(), 
              [](const Order *a, const Order *b) { return std::abs(a->qty) > std::abs(b->qty); });
    
    // Print level summary header
    std::cout << "\033[35m[ANOMALY_LEVEL] " << format_time() 
              << " Level=" << level->price << " ExpectedSide=" << (is_bid_side ? "BID" : "ASK")
              << " NetQty=" << level->net_quantity << " TotalOrders=" << level->order_count
              << " UnmatchedOrders=" << anomaly_orders.size()
              << " | TOB: Bid=" << best_bid_ << " Ask=" << best_ask_ << "\033[0m\n";
    
    // Print all unmatched orders sorted by size
    for (size_t i = 0; i < anomaly_orders.size(); ++i) {
      const Order *order = anomaly_orders[i];
      std::cout << "\033[35m  [" << (i + 1) << "] ID=" << order->id 
                << " Qty=" << order->qty
                << " Created=" << format_timestamp(order->timestamp)
                << " Age=" << calc_age_ms(order->timestamp) << "ms\033[0m\n";
    }
  }

#endif // DEBUG_ANOMALY_PRINT

  // ========================================================================================
  // DEBUG UTILITIES - DISPLAY BOOK
  // ========================================================================================

  // Display current market depth
  void inline print_book() const {
#if DEBUG_BOOK_BY_SECOND == 0
    // Print by tick
    if (new_tick_ && DEBUG_BOOK_PRINT) {
#else
    // Print every N seconds (extract second timestamp by removing millisecond)
    const uint32_t curr_second = (curr_tick_ >> 8);
    const uint32_t prev_second = (prev_tick_ >> 8);
    const bool should_print = (curr_second / DEBUG_BOOK_BY_SECOND) != (prev_second / DEBUG_BOOK_BY_SECOND);
    if (should_print && DEBUG_BOOK_PRINT) {
#endif
      std::ostringstream book_output;
      book_output << "[" << format_time() << "] [" << std::setfill('0') << std::setw(3) << total_deferred() << std::setfill(' ') << "] ";

      using namespace BookDisplay;

      update_tob();
      
#if DEBUG_ANOMALY_PRINT
      // At continuous trading start (09:30:00), scan all existing levels
      // This ensures anomalies that existed during call auction are detected
      static uint32_t last_check_second = 0;
      const uint32_t curr_second = (curr_tick_ >> 8);  // Remove milliseconds
      if (curr_second != last_check_second) {
        last_check_second = curr_second;
        const uint8_t hour = (curr_tick_ >> 24) & 0xFF;
        const uint8_t minute = (curr_tick_ >> 16) & 0xFF;
        const uint8_t second = (curr_tick_ >> 8) & 0xFF;
        
        using namespace TradingSession;
        if (hour == CONTINUOUS_TRADING_START_HOUR && 
            minute == CONTINUOUS_TRADING_START_MINUTE && 
            second == 0) {
          debug_.printed_anomalies.clear();
          refresh_cache_if_dirty();
          for (const Price price : cached_visible_prices_) {
            Level *level = find_level(price);
            if (level && level->has_visible_quantity()) {
              check_anomaly(level);
            }
          }
        }
      }
#endif

      // Collect ask levels
      std::vector<std::pair<Price, Quantity>> ask_data;
      for_each_visible_ask([&](Price price, Quantity quantity) {
        ask_data.emplace_back(price, quantity);
      },
                           MAX_DISPLAY_LEVELS);

      // Reverse ask data for display
      std::reverse(ask_data.begin(), ask_data.end());

      // Display ask levels (left side, negate for display)
      book_output << "ASK: ";
      size_t ask_empty_spaces = MAX_DISPLAY_LEVELS - ask_data.size();
      for (size_t i = 0; i < ask_empty_spaces; ++i) {
        book_output << std::setw(LEVEL_WIDTH) << " ";
      }
      for (size_t i = 0; i < ask_data.size(); ++i) {
        const Price price = ask_data[i].first;
        const Quantity qty = ask_data[i].second;
        const Quantity display_qty = -qty; // Negate: normal negative -> positive, anomaly positive -> negative
        const bool is_anomaly = (qty > 0); // Ask should be negative, positive is anomaly
        
#if DEBUG_BOOK_AS_AMOUNT == 0
        // Display as 手 (lots)
        const std::string qty_str = std::to_string(display_qty);
#else
        // Display as N万元 (N * 10000 yuan): 手 * 100 * 股价 / (N * 10000)
        const double amount = std::abs(display_qty) * 100.0 * price / (DEBUG_BOOK_AS_AMOUNT * 10000.0);
        const std::string qty_str = (display_qty < 0 ? "-" : "") + std::to_string(static_cast<int>(amount + 0.5));
#endif
        const std::string level_str = std::to_string(price) + "x" + qty_str;
        
        if (is_anomaly) {
          book_output << "\033[31m" << std::setw(LEVEL_WIDTH) << std::left << level_str << "\033[0m";
        } else {
          book_output << std::setw(LEVEL_WIDTH) << std::left << level_str;
        }
      }

      book_output << "| BID: ";

      // Collect bid levels
      std::vector<std::pair<Price, Quantity>> bid_data;
      for_each_visible_bid([&](Price price, Quantity quantity) {
        bid_data.emplace_back(price, quantity);
      },
                           MAX_DISPLAY_LEVELS);

      // Display bid levels (right side, display as-is)
      for (size_t i = 0; i < MAX_DISPLAY_LEVELS; ++i) {
        if (i < bid_data.size()) {
          const Price price = bid_data[i].first;
          const Quantity qty = bid_data[i].second;
          const bool is_anomaly = (qty < 0); // Bid should be positive, negative is anomaly
          
#if DEBUG_BOOK_AS_AMOUNT == 0
          // Display as 手 (lots)
          const std::string qty_str = std::to_string(qty);
#else
          // Display as N万元 (N * 10000 yuan): 手 * 100 * 股价 / (N * 10000)
          const double amount = std::abs(qty) * 100.0 * price / (DEBUG_BOOK_AS_AMOUNT * 10000.0);
          const std::string qty_str = (qty < 0 ? "-" : "") + std::to_string(static_cast<int>(amount + 0.5));
#endif
          const std::string level_str = std::to_string(price) + "x" + qty_str;
          
          if (is_anomaly) {
            book_output << "\033[31m" << std::setw(LEVEL_WIDTH) << std::left << level_str << "\033[0m";
          } else {
            book_output << std::setw(LEVEL_WIDTH) << std::left << level_str;
          }
        } else {
          book_output << std::setw(LEVEL_WIDTH) << " ";
        }
      }

      // Count anomalies across ALL visible levels (not limited to displayed levels)
      size_t anomaly_count = 0;
      refresh_cache_if_dirty();
      const Price tob_mid = (best_bid_ + best_ask_) / 2;
      for (const Price price : cached_visible_prices_) {
        const Level *level = find_level(price);
        if (!level || !level->has_visible_quantity()) continue;
        
        // Classify by price relative to TOB mid price
        const bool is_bid_side = (price < tob_mid);
        
        // Check for sign anomaly: BID should be positive, ASK should be negative
        if ((is_bid_side && level->net_quantity < 0) || (!is_bid_side && level->net_quantity > 0)) {
          ++anomaly_count;
        }
      }
      
      if (anomaly_count > 0) {
        book_output << " \033[31m[" << anomaly_count << " anomalies]\033[0m";
      }
      
      std::cout << book_output.str() << "\n";

#if DEBUG_ANOMALY_PRINT
      // Check anomalies for ALL visible levels (proactive detection)
      for (const Price price : cached_visible_prices_) {
        Level *level = find_level(price);
        if (level && level->has_visible_quantity()) {
          check_anomaly(level);
        }
      }
#endif
    }
#if DEBUG_ORDER_PRINT
    char order_type_char = (order.order_type == L2::OrderType::MAKER) ? 'M' : (order.order_type == L2::OrderType::CANCEL) ? 'C'
                                                                                                                          : 'T';
    char order_dir_char = (order.order_dir == L2::OrderDirection::BID) ? 'B' : 'S';
    std::cout << "[" << format_time() << "] " << " ID: " << get_target_id(order) << " Type: " << order_type_char << " Direction: " << order_dir_char << " Price: " << order.price << " Volume: " << order.volume << std::endl;
#endif
  }
};
