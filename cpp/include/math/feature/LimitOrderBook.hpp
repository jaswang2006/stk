// 2021-Deep Learning for Market by Order Data
// https://arxiv.org/abs/2102.08811

// 2025-An Efficient deep learning model to Predict Stock Price Movement Based on Limit Order Book
// https://arxiv.org/abs/2505.22678

#pragma once

#include "define/CBuffer.hpp"
#include "define/Dtype.hpp"
// #include "misc/print.hpp"

#include "math/normalize/RollingZScore.hpp"
#include <cmath>

#define TICK_SIZE 0.01f

template <size_t N> // compiler auto derive
class LimitOrderBook {
public:
  // Constructor
  explicit LimitOrderBook(
      CBuffer<uint16_t, N> *delta_t,
      CBuffer<float, N> *prices,
      CBuffer<float, N> *volumes,
      CBuffer<float, N> *turnovers,
      CBuffer<float, N> *vwaps,
      CBuffer<uint8_t, N> *directions,
      CBuffer<float, N> *spreads,
      CBuffer<float, N> *mid_prices)
      : delta_t_(delta_t),
        prices_(prices),
        volumes_(volumes),
        turnovers_(turnovers),
        vwaps_(vwaps),
        directions_(directions),
        spreads_(spreads),
        mid_prices_(mid_prices) {}

  inline void update(Table::Snapshot_Record &snapshot, bool is_session_start) {
    const uint16_t delta_t = static_cast<uint16_t>(is_session_start ? 0 : (snapshot.seconds_in_day - last_seconds_in_day));
    const float best_bid_price = snapshot.bid_price_ticks[0];
    const float best_ask_price = snapshot.ask_price_ticks[0];
    const float best_bid_volume = snapshot.bid_volumes[0];
    const float best_ask_volume = snapshot.ask_volumes[0];

    // Calculate derived metrics with conditional moves instead of branches
    const float mid_price = (best_bid_price + best_ask_price) * 0.5f;

    // Optimize volume calculations
    const float volume = static_cast<float>(snapshot.volume) * 100.0f; // hands -> shares
    const float turnover = static_cast<float>(snapshot.turnover);
    const float vwap = (volume > 0) ? (turnover / volume) : vwaps_->back();
    // first check vwap dir(avg price up/down), if equal, use last trade dir during n seconds
    const uint8_t dir = static_cast<uint8_t>(is_session_start ? 0 : (vwap == vwaps_->back() ? snapshot.direction : (vwap < vwaps_->back() ? 1 : 0)));

    // Order Book Imbalance =======================================================================

    // spread
    const float spread = best_ask_price - best_bid_price;
    snapshot.spread_z = zs_spread_.update(spread);

    // micro-price-gap
    const float denom = best_bid_volume + best_ask_volume;
    const float micro_price = (denom > 0.0f)
                                  ? ((best_ask_price * best_bid_volume +
                                      best_bid_price * best_ask_volume) /
                                     denom)
                                  : mid_price;
    snapshot.mpg_z = zs_mpg_.update(micro_price - mid_price);

    // top-of-book imbalance
    snapshot.tobi_z = zs_tobi_.update((best_bid_volume - best_ask_volume) / (best_bid_volume + best_ask_volume));

    // convexity-weighted multi-level imbalance
    float cwi_numer[CWI_N]{};
    float cwi_denom[CWI_N]{};
    for (int i = 0; i < 5; ++i) {
      const float v_bid = static_cast<float>(snapshot.bid_volumes[i]);
      const float v_ask = static_cast<float>(snapshot.ask_volumes[i]);
      const float level_index = static_cast<float>(i + 1);
      for (int k = 0; k < CWI_N; ++k) {
        const float weight = 1.0f / std::pow(level_index, CWI_GAMMA[k]);
        cwi_numer[k] += weight * v_bid - weight * v_ask;
        cwi_denom[k] += weight * v_bid + weight * v_ask;
      }
    }
    for (int k = 0; k < CWI_N; ++k) {
      snapshot.cwi_z[k] = zs_cwi_[k].update(cwi_numer[k] / cwi_denom[k]);
    }

    // distance–discounted multi-level imbalance
    float ddi_numer[DDI_N]{}; // init all to 0.0f
    float ddi_denom[DDI_N]{}; // init all to 0.0f
    for (int i = 0; i < 5; ++i) {
      // distance from each level price to mid, normalized by tick size (in ticks)
      const float price_distance_bid = (mid_price - snapshot.bid_price_ticks[i]) / TICK_SIZE;
      const float price_distance_ask = (snapshot.ask_price_ticks[i] - mid_price) / TICK_SIZE;
      const float v_bid = static_cast<float>(snapshot.bid_volumes[i]);
      const float v_ask = static_cast<float>(snapshot.ask_volumes[i]);
      for (int k = 0; k < DDI_N; ++k) {
        const float weight_bid = std::exp(-DDI_LAMBDAS[k] * price_distance_bid);
        const float weight_ask = std::exp(-DDI_LAMBDAS[k] * price_distance_ask);
        ddi_numer[k] += weight_bid * v_bid - weight_ask * v_ask;
        ddi_denom[k] += weight_bid * v_bid + weight_ask * v_ask;
      }
    }
    for (int k = 0; k < DDI_N; ++k) { // sum over all layers first
      snapshot.ddi_z[k] = zs_ddi_[k].update(ddi_numer[k] / ddi_denom[k]);
    }


    // Order Book dynamics =======================================================================0

    // Batch update analysis buffers for better cache locality
    delta_t_->push_back(delta_t);
    prices_->push_back(snapshot.latest_price_tick);
    volumes_->push_back(volume);
    turnovers_->push_back(turnover);
    vwaps_->push_back(vwap);
    directions_->push_back(dir);
    spreads_->push_back(spread);
    mid_prices_->push_back(mid_price);

    last_seconds_in_day = snapshot.seconds_in_day;
  }

private:
  uint32_t last_seconds_in_day = 0;
  uint8_t last_ = 0;

  CBuffer<uint16_t, N> *delta_t_;
  CBuffer<float, N> *prices_;
  CBuffer<float, N> *volumes_;
  CBuffer<float, N> *turnovers_;
  CBuffer<float, N> *vwaps_;
  CBuffer<uint8_t, N> *directions_;
  CBuffer<float, N> *spreads_;
  CBuffer<float, N> *mid_prices_;

  RollingZScore<float, int(30 * 60 / snapshot_interval)> zs_spread_;
  RollingZScore<float, int(30 * 60 / snapshot_interval)> zs_mpg_;
  RollingZScore<float, int(30 * 60 / snapshot_interval)> zs_tobi_;
  RollingZScore<float, int(30 * 60 / snapshot_interval)> zs_cwi_[CWI_N];
  RollingZScore<float, int(30 * 60 / snapshot_interval)> zs_ddi_[DDI_N];
};
