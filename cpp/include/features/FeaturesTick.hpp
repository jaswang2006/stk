#pragma once

#include "lob/LimitOrderBookDefine.hpp"
#include "math/normalize/RollingZScore.hpp"
#include <cmath>

//========================================================================================
// CONFIGURATION
//========================================================================================

#define TICK_SIZE 0.01f

// Convexity-weighted imbalance parameters
static constexpr int CWI_N = 3;
static constexpr float CWI_GAMMA[CWI_N] = {1.0f, 2.0f, 3.0f};

// Distance-discounted imbalance parameters
static constexpr int DDI_N = 3;
static constexpr float DDI_LAMBDAS[DDI_N] = {0.01f, 0.05f, 0.1f};

// Rolling Z-Score window size (30 minutes, assuming some interval)
// TODO: 根据实际的 tick 频率调整
static constexpr int ZSCORE_WINDOW = 1800;

//========================================================================================
// FeaturesTick CLASS
//========================================================================================
// 
// 逐笔特征计算类
// - 从 LOB_Feature 结构体读取盘口数据
// - 计算各种订单簿特征（imbalance, spread, micro-price等）
// - 使用 RollingZScore 进行标准化
// - 不维护输入数据，只负责计算
//
//========================================================================================

class FeaturesTick {
public:
  //======================================================================================
  // CONSTRUCTOR
  //======================================================================================
  
  explicit FeaturesTick(const LOB_Feature* lob_feature) 
      : lob_feature_(lob_feature) {}

  //======================================================================================
  // FEATURE CALCULATION
  //======================================================================================
  
  // 主更新函数：从 LOB_Feature 读取数据，计算所有特征
  // 
  // 计算的特征：
  //   - spread: 买卖价差
  //   - mpg (micro-price-gap): 微观价格与中间价之差
  //   - tobi (top-of-book imbalance): 顶层盘口失衡
  //   - cwi (convexity-weighted imbalance): 凸加权多层失衡
  //   - ddi (distance-discounted imbalance): 距离折扣多层失衡
  //
  void update() {
    const auto& depth_buffer = lob_feature_->depth_buffer;
    
    // 检查 depth_buffer 是否有足够的数据
    if (depth_buffer.size() < 2 * LOB_FEATURE_DEPTH_LEVELS) {
      return; // 深度不足，跳过计算
    }

    // 提取 best bid/ask (depth_buffer 中间位置)
    // CBuffer: [0]:卖N, [N-1]:卖1, [N]:买1, ..., [2N-1]:买N
    const Level* best_ask_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS - 1];
    const Level* best_bid_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS];
    
    if (!best_ask_level || !best_bid_level) {
      return; // 无效数据
    }

    const float best_bid_price = static_cast<float>(best_bid_level->price) * 0.01f;
    const float best_ask_price = static_cast<float>(best_ask_level->price) * 0.01f;
    const float best_bid_volume = static_cast<float>(std::abs(best_bid_level->net_quantity));
    const float best_ask_volume = static_cast<float>(std::abs(best_ask_level->net_quantity));

    // 计算基础衍生指标
    const float mid_price = (best_bid_price + best_ask_price) * 0.5f;
    const float spread = best_ask_price - best_bid_price;

    // ==================================================================================
    // Order Book Imbalance Features
    // ==================================================================================

    // 1. Spread (价差)
    float spread_z = zs_spread_.update(spread);

    // 2. Micro-price-gap (微观价格偏差)
    const float denom = best_bid_volume + best_ask_volume;
    const float micro_price = (denom > 0.0f)
                                  ? ((best_ask_price * best_bid_volume +
                                      best_bid_price * best_ask_volume) /
                                     denom)
                                  : mid_price;
    float mpg_z = zs_mpg_.update(micro_price - mid_price);

    // 3. Top-of-book imbalance (顶层失衡)
    const float tobi_denom = best_bid_volume + best_ask_volume;
    float tobi_z = (tobi_denom > 0.0f) 
                      ? zs_tobi_.update((best_bid_volume - best_ask_volume) / tobi_denom)
                      : 0.0f;

    // 4. Convexity-weighted multi-level imbalance (凸加权多层失衡)
    float cwi_z[CWI_N] = {};
    {
      float cwi_numer[CWI_N] = {};
      float cwi_denom[CWI_N] = {};
      
      // 遍历多档深度
      const int max_levels = std::min(5, static_cast<int>(LOB_FEATURE_DEPTH_LEVELS));
      for (int i = 0; i < max_levels; ++i) {
        // Bid side: depth_buffer[LOB_FEATURE_DEPTH_LEVELS + i]
        // Ask side: depth_buffer[LOB_FEATURE_DEPTH_LEVELS - 1 - i]
        const Level* bid_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS + i];
        const Level* ask_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS - 1 - i];
        
        if (!bid_level || !ask_level) break;
        
        const float v_bid = static_cast<float>(std::abs(bid_level->net_quantity));
        const float v_ask = static_cast<float>(std::abs(ask_level->net_quantity));
        const float level_index = static_cast<float>(i + 1);
        
        for (int k = 0; k < CWI_N; ++k) {
          const float weight = 1.0f / std::pow(level_index, CWI_GAMMA[k]);
          cwi_numer[k] += weight * v_bid - weight * v_ask;
          cwi_denom[k] += weight * v_bid + weight * v_ask;
        }
      }
      
      for (int k = 0; k < CWI_N; ++k) {
        cwi_z[k] = (cwi_denom[k] > 0.0f) 
                      ? zs_cwi_[k].update(cwi_numer[k] / cwi_denom[k])
                      : 0.0f;
      }
    }

    // 5. Distance-discounted multi-level imbalance (距离折扣多层失衡)
    float ddi_z[DDI_N] = {};
    {
      float ddi_numer[DDI_N] = {};
      float ddi_denom[DDI_N] = {};
      
      const int max_levels = std::min(5, static_cast<int>(LOB_FEATURE_DEPTH_LEVELS));
      for (int i = 0; i < max_levels; ++i) {
        const Level* bid_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS + i];
        const Level* ask_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS - 1 - i];
        
        if (!bid_level || !ask_level) break;
        
        const float bid_price = static_cast<float>(bid_level->price) * 0.01f;
        const float ask_price = static_cast<float>(ask_level->price) * 0.01f;
        
        // 距离计算：从各档价格到中间价的距离（以tick为单位）
        const float price_distance_bid = (mid_price - bid_price) / TICK_SIZE;
        const float price_distance_ask = (ask_price - mid_price) / TICK_SIZE;
        
        const float v_bid = static_cast<float>(std::abs(bid_level->net_quantity));
        const float v_ask = static_cast<float>(std::abs(ask_level->net_quantity));
        
        for (int k = 0; k < DDI_N; ++k) {
          const float weight_bid = std::exp(-DDI_LAMBDAS[k] * price_distance_bid);
          const float weight_ask = std::exp(-DDI_LAMBDAS[k] * price_distance_ask);
          ddi_numer[k] += weight_bid * v_bid - weight_ask * v_ask;
          ddi_denom[k] += weight_bid * v_bid + weight_ask * v_ask;
        }
      }
      
      for (int k = 0; k < DDI_N; ++k) {
        ddi_z[k] = (ddi_denom[k] > 0.0f)
                      ? zs_ddi_[k].update(ddi_numer[k] / ddi_denom[k])
                      : 0.0f;
      }
    }

    // ==================================================================================
    // 特征计算完成
    // TODO: 存储特征值（后续实现）
    // ==================================================================================
    
    // 暂存计算结果到成员变量（供外部查询）
    last_spread_z_ = spread_z;
    last_mpg_z_ = mpg_z;
    last_tobi_z_ = tobi_z;
    for (int k = 0; k < CWI_N; ++k) {
      last_cwi_z_[k] = cwi_z[k];
    }
    for (int k = 0; k < DDI_N; ++k) {
      last_ddi_z_[k] = ddi_z[k];
    }
  }

  //======================================================================================
  // QUERY INTERFACE (查询最近计算的特征)
  //======================================================================================

  float get_spread_z() const { return last_spread_z_; }
  float get_mpg_z() const { return last_mpg_z_; }
  float get_tobi_z() const { return last_tobi_z_; }
  float get_cwi_z(int k) const { return (k >= 0 && k < CWI_N) ? last_cwi_z_[k] : 0.0f; }
  float get_ddi_z(int k) const { return (k >= 0 && k < DDI_N) ? last_ddi_z_[k] : 0.0f; }

private:
  //======================================================================================
  // LOB FEATURE POINTER
  //======================================================================================
  
  const LOB_Feature* lob_feature_;  // 指向 LOB_Feature，只读访问

  //======================================================================================
  // ROLLING Z-SCORE NORMALIZERS
  //======================================================================================
  
  RollingZScore<float, ZSCORE_WINDOW> zs_spread_;
  RollingZScore<float, ZSCORE_WINDOW> zs_mpg_;
  RollingZScore<float, ZSCORE_WINDOW> zs_tobi_;
  RollingZScore<float, ZSCORE_WINDOW> zs_cwi_[CWI_N];
  RollingZScore<float, ZSCORE_WINDOW> zs_ddi_[DDI_N];

  //======================================================================================
  // LAST COMPUTED FEATURES (for query)
  //======================================================================================
  
  float last_spread_z_ = 0.0f;
  float last_mpg_z_ = 0.0f;
  float last_tobi_z_ = 0.0f;
  float last_cwi_z_[CWI_N] = {};
  float last_ddi_z_[DDI_N] = {};
};
