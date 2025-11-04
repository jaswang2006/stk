#pragma once

#include "lob/LimitOrderBookDefine.hpp"
#include "math/normalize/RollingZScore.hpp"
#include <cmath>
#include <string_view>

// 核心理念: 在低信噪比、强竞争的二级市场，端到端的深度模型(数据先验)会先被淘汰, 特征工程因子挖掘(结构性先验)是生存条件，不是选择。

// Feature class level 1 name mapping
struct FeatureClass1Name {
  std::string_view name_cn;
  std::string_view name_en;
};

// Feature class level 2 name mapping
struct FeatureClass2Name {
  std::string_view name_cn;
  std::string_view name_en;
};

// clang-format off
constexpr std::pair<std::string_view, FeatureClass1Name> FeatureClass1Map[] = {
  {"SE", {"快照扩展",           "Snapshot Extensions"}},
  {"DT", {"动态/时间变体",      "Dynamic/Temporal"}},
  {"SG", {"深度形状/几何",      "Shape/Geometry"}},
  {"FV", {"流量/场所条件",      "Flow/Venue"}},
  {"OA", {"订单侵略性",         "Order Aggressiveness"}},
  {"OS", {"订单簿形状",         "Order Book Shape"}},
  {"CB", {"撤单行为",           "Cancellation Behavior"}},
  {"EC", {"事件聚集",           "Event Clustering"}},
  {"LR", {"订单簿韧性",         "LOB Resiliency"}},
  {"AO", {"异常挂单",           "Abnormal Orders"}},
  {"TI", {"逐笔主动交易",       "Trade Initiation"}},
  {"BS", {"基础特征",           "Basic"}},
};

constexpr std::pair<std::string_view, FeatureClass2Name> FeatureClass2Map[] = {
  // Snapshot Extensions (SE)
  {"SE_MLI",  {"多层失衡",         "Multi-level Imbalance"}},
  {"SE_SA",   {"价差调整",         "Spread-adjusted"}},
  {"SE_EP",   {"执行概率",         "Execution Probability"}},
  {"SE_AS",   {"逆向选择",         "Adverse Selection"}},
  {"SE_MP",   {"微观价格",         "Microprice"}},
  // Dynamic/Temporal (DT)
  {"DT_MOM",  {"动量",             "Momentum"}},
  {"DT_VOL",  {"波动率",           "Volatility"}},
  {"DT_STG",  {"稳定性门控",       "Stability Gate"}},
  {"DT_INN",  {"创新/惊喜",        "Innovation"}},
  {"DT_REV",  {"回归",             "Reversion"}},
  {"DT_QP",   {"队列位置",         "Queue Position"}},
  {"DT_REG",  {"机制切换",         "Regime"}},
  // Shape/Geometry (SG)
  {"SG_SLP",  {"斜率",             "Slope"}},
  {"SG_CVX",  {"凸性",             "Convexity"}},
  {"SG_PG",   {"价格间距",         "Price Gap"}},
  {"SG_ENT",  {"熵",               "Entropy"}},
  // Flow/Venue (FV)
  {"FV_AF",   {"到达流",           "Arrival Flow"}},
  {"FV_ACF",  {"挂撤流",           "Add-Cancel Flow"}},
  {"FV_VEN",  {"场所",             "Venue"}},
  {"FV_CA",   {"跨资产",           "Cross-asset"}},
  {"FV_SR",   {"扫单风险",         "Sweep Risk"}},
  // Order Aggressiveness (OA)
  {"OA_UNI",  {"单边侵略性",       "Unilateral"}},
  {"OA_DIF",  {"侵略性差",         "Difference"}},
  {"OA_TT",   {"时间趋势",         "Time Trend"}},
  {"OA_EP",   {"执行概率",         "Execution Probability"}},
  // Order Book Shape (OS)
  {"OS_POS",  {"峰位置",           "Hump Position"}},
  {"OS_SLP",  {"斜率",             "Slope"}},
  {"OS_CVX",  {"凸性",             "Convexity"}},
  {"OS_PCN",  {"峰数",             "Peak Count"}},
  {"OS_EXT",  {"极端档异常",       "Extreme Level"}},
  {"OS_EVO",  {"时间演化",         "Evolution"}},
  // Cancellation Behavior (CB)
  {"CB_VOL",  {"撤单量",           "Cancel Volume"}},
  {"CB_DNS",  {"撤单分布",         "Density"}},
  {"CB_CPR",  {"撤挂比",           "Cancel-Post Ratio"}},
  {"CB_LCN",  {"大单撤单",         "Large Cancel"}},
  {"CB_PTC",  {"成交前撤单",       "Pre-trade Cancel"}},
  {"CB_FLT",  {"闪单",             "Fleeting"}},
  {"CB_SPF",  {"欺骗",             "Spoofing"}},
  // Event Clustering (EC)
  {"EC_CCL",  {"撤单聚集",         "Cancel Clustering"}},
  {"EC_OCL",  {"订单聚集",         "Order Clustering"}},
  {"EC_IDX",  {"事件指数",         "Event Index"}},
  {"EC_MCL",  {"多事件聚集",       "Multi-event Clustering"}},
  {"EC_DEP",  {"时序依赖",         "Dependency"}},
  // LOB Resiliency (LR)
  {"LR_VOL",  {"恢复量",           "Replenish Volume"}},
  {"LR_RT",   {"恢复率",           "Resiliency Ratio"}},
  {"LR_SPD",  {"恢复速度",         "Resiliency Speed"}},
  {"LR_ASM",  {"恢复不对称",       "Resiliency Asymmetry"}},
  {"LR_DCY",  {"韧性衰减",         "Resiliency Decay"}},
  // Abnormal Orders (AO)
  {"AO_LMT",  {"涨跌停",           "Limit Price"}},
  {"AO_STL",  {"长时间未成交",     "Stale"}},
  {"AO_DUR",  {"持续时间长",       "Durable"}},
  {"AO_BLK",  {"异常密集",         "Blockage"}},
  // Trade Initiation (TI)
  {"TI_DIR",  {"主动方向",         "Direction"}},
  {"TI_BR",   {"主买比例",         "Buy Ratio"}},
  {"TI_NF",   {"净流入",           "Net Flow"}},
  {"TI_LBR",  {"大单主买",         "Large Buy Ratio"}},
  {"TI_SW",   {"方向切换",         "Switch"}},
  // Basic (BS)
  {"BS_P",    {"价格",             "Price"}},
  {"BS_I",    {"失衡",             "Imbalance"}},
};
// clang-format on

struct FeaturesTickMeta {
  std::string_view code;         // 特征简写
  std::string_view name_cn;      // 特征名称(中文)
  std::string_view name_en;      // 特征名称(英文)
  std::string_view class_level1; // 一级特征类型简写
  std::string_view class_level2; // 二级特征类型简写
  std::string_view description;  // 描述
  std::string_view formula;      // 公式
};

// clang-format off
constexpr FeaturesTickMeta FeaturesTick_Schema[] = {
    // ========== SE: Snapshot Extensions (快照扩展) ==========
    {"cwi_g1",      "凸加权失衡γ=1",       "Convexity-weighted Imbalance γ=1",        "SE", "SE_MLI", "凸加权多层失衡，γ=1.0",              "I(cvx) = Σw_i*(V_bid - V_ask) / Σw_i*(V_bid + V_ask), w_i=1/(i+ε)^γ"},
    {"cwi_g2",      "凸加权失衡γ=2",       "Convexity-weighted Imbalance γ=2",        "SE", "SE_MLI", "凸加权多层失衡，γ=2.0",              "I(cvx) = Σw_i*(V_bid - V_ask) / Σw_i*(V_bid + V_ask), w_i=1/(i+ε)^γ"},
    {"cwi_g3",      "凸加权失衡γ=3",       "Convexity-weighted Imbalance γ=3",        "SE", "SE_MLI", "凸加权多层失衡，γ=3.0",              "I(cvx) = Σw_i*(V_bid - V_ask) / Σw_i*(V_bid + V_ask), w_i=1/(i+ε)^γ"},
    {"ddi_l1",      "距离折扣失衡λ=0.01",  "Distance-discounted Imbalance λ=0.01",    "SE", "SE_MLI", "距离折扣多层失衡，λ=0.01",           "I(λ) = Σe^(-λΔp)*(V_bid - V_ask) / Σe^(-λΔp)*(V_bid + V_ask)"},
    {"ddi_l2",      "距离折扣失衡λ=0.05",  "Distance-discounted Imbalance λ=0.05",    "SE", "SE_MLI", "距离折扣多层失衡，λ=0.05",           "I(λ) = Σe^(-λΔp)*(V_bid - V_ask) / Σe^(-λΔp)*(V_bid + V_ask)"},
    {"ddi_l3",      "距离折扣失衡λ=0.1",   "Distance-discounted Imbalance λ=0.1",     "SE", "SE_MLI", "距离折扣多层失衡，λ=0.1",            "I(λ) = Σe^(-λΔp)*(V_bid - V_ask) / Σe^(-λΔp)*(V_bid + V_ask)"},
    {"spr_imb",     "价差感知失衡",        "Spread-aware Imbalance",                  "SE", "SE_SA",  "价差对失衡信号的调整",               "I(spr) = I(N) * (spread <= s0 ? 1 : 1/(1+spread/tick))"},
    {"hid_imb",     "隐藏流动性失衡",      "Hidden-liquidity Adjusted Imbalance",     "SE", "SE_EP",  "基于执行概率调整的失衡",             "I(hid) = Σ(V_bid*π_bid - V_ask*π_ask) / Σ(V_bid*π_bid + V_ask*π_ask)"},
    {"as_imb",      "逆向选择惩罚失衡",    "Adverse-selection Penalized Imbalance",   "SE", "SE_AS",  "考虑逆向选择风险的失衡",             "I(as) = I(N) - β*AS_t"},
    {"mpg_z",       "微观价格偏差z值",     "Microprice Gap Z-score",                  "SE", "SE_MP",  "微观价格与中间价的标准化偏差",       "Z(mp) = (MP - Mid) / σ(MP-Mid), MP=(P_ask*V_bid+P_bid*V_ask)/(V_bid+V_ask)"},

    // ========== DT: Dynamic/Temporal (动态/时间变体) ==========
    {"imb_mom",     "失衡动量",            "Imbalance Momentum",                      "DT", "DT_MOM", "失衡变化的指数加权动量",             "M(I) = Σα_k*ΔI_{t-k}, ΔI_t = I_t - I_{t-1}"},
    {"imb_vol",     "失衡波动率",          "Imbalance Volatility",                    "DT", "DT_VOL", "失衡的滚动波动率",                   "σ(I) = sqrt(Σω_k*(I_{t-k} - Ī)^2)"},
    {"imb_stab",    "失衡稳定性",          "Imbalance Stability",                     "DT", "DT_STG", "仅在波动率低时的失衡信号",           "I(stab) = I(N) * 1{σ(I) <= q_η}"},
    {"imb_surp",    "失衡惊喜",            "Imbalance Surprise",                      "DT", "DT_INN", "失衡相对预测的残差",                 "S(I) = I_t - E[I_t | F_{t-1}]"},
    {"imb_rev",     "失衡回归压力",        "Imbalance Reversion Pressure",            "DT", "DT_REV", "失衡极端值后的回归趋势",             "R(I) = -∇I_t, ∇I_t = I_t - I_{t-Δ}"},
    {"imb_qp",      "队列位置失衡漂移",    "Queue-position Imbalance Drift",          "DT", "DT_QP",  "根据队列位置调整的失衡",             "I(qp) = I(N) * (1 - T_fill/τ_0)"},
    {"imb_reg",     "机制切换失衡",        "Regime-switched Imbalance",               "DT", "DT_REG", "多机制下的失衡加权",                 "I(reg) = Σ_r P(R_t=r)*I(N)"},

    // ========== SG: Shape/Geometry (深度形状/几何) ==========
    {"dep_slp",     "深度斜率失衡",        "Depth-slope Imbalance",                   "SG", "SG_SLP", "买卖两侧深度梯度差异",               "I(slope) = (b_bid - b_ask) / (|b_bid| + |b_ask|)"},
    {"dep_cvx",     "深度凸性失衡",        "Depth-convexity Imbalance",               "SG", "SG_CVX", "买卖两侧深度凸性差异",               "I(cvx-shape) = sign(c2_bid - c2_ask) * Σ(V_bid+V_ask)/|c2_bid-c2_ask|"},
    {"pgap_imb",    "价格间距加权失衡",    "Price-gap Weighted Imbalance",            "SG", "SG_PG",  "根据价格距离加权的失衡",             "I(Δp) = Σ(Δp)^ρ*(V_bid-V_ask) / Σ(Δp)^ρ*(V_bid+V_ask)"},
    {"ent_bid",     "买侧深度熵",          "Bid Side Entropy",                        "SG", "SG_ENT", "买侧深度分布的信息熵",               "H_bid = -Σπ_i*log(π_i), π_i=V_i/ΣV_j"},
    {"ent_ask",     "卖侧深度熵",          "Ask Side Entropy",                        "SG", "SG_ENT", "卖侧深度分布的信息熵",               "H_ask = -Σπ_i*log(π_i), π_i=V_i/ΣV_j"},
    {"ent_imb",     "熵失衡",              "Entropy Imbalance",                       "SG", "SG_ENT", "买卖两侧熵的差异",                   "I(ent) = (H_ask - H_bid) / (H_ask + H_bid)"},

    // ========== FV: Flow/Venue (流量/场所条件) ==========
    {"arr_imb",     "到达强度失衡",        "Arrival-intensity Adjusted Imbalance",    "FV", "FV_AF",  "结合订单到达强度的失衡",             "I(λ-adj) = I(N) * (λ_buy - λ_sell) / (λ_buy + λ_sell)"},
    {"ac_imb",      "挂撤流失衡",          "Add-Cancel Flow Imbalance",               "FV", "FV_ACF", "净挂单流（挂单-撤单）失衡",          "I(netflow) = Σ(A_bid-C_bid) - Σ(A_ask-C_ask) / Σ|A-C|"},
    {"ven_imb",     "场所加权失衡",        "Venue-weighted Imbalance",                "FV", "FV_VEN", "多场所加权的失衡",                   "I(venue) = Σw_v*I(N)_v"},
    {"lead_imb",    "领先滞后失衡",        "Lead-lag Cross-asset Imbalance",          "FV", "FV_CA",  "跨资产领先滞后失衡",                 "I_{A->B} = Σθ_l*I_{A,t-l}"},
    {"swp_imb",     "扫单风险失衡",        "Sweep Risk Imbalance",                    "FV", "FV_SR",  "考虑扫单风险的失衡",                 "I(sweep) = I(N) * 1{sweep_risk_low}"},

    // ========== OA: Order Aggressiveness (订单侵略性) ==========
    {"agr_buy",     "买单平均侵略性",      "Mean Buy Aggressiveness",                 "OA", "OA_UNI", "买单距离最优价的平均侵略性",         "aggr_buy = mean(log(best_bid/order_price))"},
    {"agr_sell",    "卖单平均侵略性",      "Mean Sell Aggressiveness",                "OA", "OA_UNI", "卖单距离最优价的平均侵略性",         "aggr_sell = mean(log(order_price/best_ask))"},
    {"agr_dif",     "多空侵略性差",        "Buy-Sell Aggressiveness Delta",           "OA", "OA_DIF", "买卖侵略性均值差",                   "delta_aggr = aggr_buy - aggr_sell"},
    {"agr_slp",     "侵略性趋势斜率",      "Aggressiveness Trend Slope",              "OA", "OA_TT",  "侵略性时间序列斜率",                 "slope(aggr_t)"},
    {"agr_std",     "侵略性标准差",        "Aggressiveness Std Dev",                  "OA", "OA_TT",  "侵略性滑窗标准差",                   "std(aggr_t)"},
    {"pexec_b",     "买侧执行概率",        "Buy Execution Probability",               "OA", "OA_EP",  "基于深度的买侧执行概率",             "P(exec_buy) = f(bid_depth)"},
    {"pexec_s",     "卖侧执行概率",        "Sell Execution Probability",              "OA", "OA_EP",  "基于深度的卖侧执行概率",             "P(exec_sell) = f(ask_depth)"},

    // ========== OS: Order Book Shape (订单簿形状) ==========
    {"hump_loc_b",  "买侧峰位置",          "Bid Hump Location",                       "OS", "OS_POS", "买侧挂单量最大位置",                 "hump_loc_bid = argmax(V_bid_i)"},
    {"hump_loc_s",  "卖侧峰位置",          "Ask Hump Location",                       "OS", "OS_POS", "卖侧挂单量最大位置",                 "hump_loc_ask = argmax(V_ask_i)"},
    {"lob_slp_b",   "买侧LOB斜率",         "Bid LOB Slope",                           "OS", "OS_SLP", "买侧挂单梯度",                       "slope_bid = ΔV_bid / Δprice"},
    {"lob_slp_s",   "卖侧LOB斜率",         "Ask LOB Slope",                           "OS", "OS_SLP", "卖侧挂单梯度",                       "slope_ask = ΔV_ask / Δprice"},
    {"lob_cvx_b",   "买侧LOB凸性",         "Bid LOB Convexity",                       "OS", "OS_CVX", "买侧挂单曲率",                       "convexity_bid = c2_bid"},
    {"lob_cvx_s",   "卖侧LOB凸性",         "Ask LOB Convexity",                       "OS", "OS_CVX", "卖侧挂单曲率",                       "convexity_ask = c2_ask"},
    {"peak_cnt_b",  "买侧峰数",            "Bid Peak Count",                          "OS", "OS_PCN", "买侧挂单峰的数量",                   "n_peaks_bid = count_peaks(LOB_bid)"},
    {"peak_cnt_s",  "卖侧峰数",            "Ask Peak Count",                          "OS", "OS_PCN", "卖侧挂单峰的数量",                   "n_peaks_ask = count_peaks(LOB_ask)"},
    {"ext_vol_b",   "买侧远端异常量",      "Bid Extreme Volume",                      "OS", "OS_EXT", "远离买一的异常挂单量",               "vol_bid_at_dist_N"},
    {"ext_vol_s",   "卖侧远端异常量",      "Ask Extreme Volume",                      "OS", "OS_EXT", "远离卖一的异常挂单量",               "vol_ask_at_dist_N"},
    {"hump_drift",  "峰位置漂移",          "Hump Location Drift",                     "OS", "OS_EVO", "峰位置的变化率",                     "Δhump_loc / Δt"},
    {"shape_chg",   "形状变化率",          "Shape Change Rate",                       "OS", "OS_EVO", "LOB形状参数变化率",                  "Δshape / Δt"},

    // ========== CB: Cancellation Behavior (撤单行为) ==========
    {"canc_vol",    "撤单总量",            "Cancel Volume",                           "CB", "CB_VOL", "时间窗口内撤单总量",                 "cancel_vol"},
    {"canc_val",    "撤单总额",            "Cancel Value",                            "CB", "CB_VOL", "时间窗口内撤单总金额",               "cancel_value"},
    {"canc_cnt",    "撤单笔数",            "Cancel Count",                            "CB", "CB_VOL", "时间窗口内撤单笔数",                 "cancel_count"},
    {"canc_dns",    "撤单密度",            "Cancel Density",                          "CB", "CB_DNS", "撤单在价格档的分布集中度",           "cancel_density(p)"},
    {"cpr",         "撤挂比",              "Cancel-to-Post Ratio",                    "CB", "CB_CPR", "撤单量/挂单量",                      "cancel_vol / post_vol"},
    {"lcanc_rt",    "大单撤单比",          "Large Cancel Ratio",                      "CB", "CB_LCN", "大单撤单占比",                       "large_cancel_vol / total_cancel_vol"},
    {"ptc_rt",      "成交前撤单比",        "Pre-trade Cancel Ratio",                  "CB", "CB_PTC", "成交前撤单与成交比率",               "pre_trade_cancel / trade"},
    {"fleet_rt",    "闪单占比",            "Fleeting Order Ratio",                    "CB", "CB_FLT", "极短时间挂撤订单占比",               "fleeting_vol / total_vol (<50ms)"},
    {"spoof_flg",   "欺骗标识",            "Spoofing Flag",                           "CB", "CB_SPF", "识别欺骗性挂单行为",                 "spoof_flag = 1{vol↑ + cancel↑}"},

    // ========== EC: Event Clustering (事件聚集) ==========
    {"canc_clst",   "撤单聚集",            "Cancel Clustering",                       "EC", "EC_CCL", "单位时间撤单事件密度",               "cancel_event_count(1s)"},
    {"lord_clst",   "大单聚集",            "Large Order Clustering",                  "EC", "EC_OCL", "单位时间大挂单事件密度",             "large_order_event_rate"},
    {"evt_idx",     "高频事件指数",        "High-frequency Event Index",              "EC", "EC_IDX", "异常行为单位时间指数",               "event_rate"},
    {"evt_ent",     "事件熵",              "Event Entropy",                           "EC", "EC_IDX", "事件类型分布的熵",                   "event_entropy"},
    {"multi_clst",  "多事件联合聚集",      "Multi-event Cluster Score",               "EC", "EC_MCL", "多种事件同时发生的评分",             "multi_event_cluster_score"},
    {"evt_dep",     "事件依赖性",          "Event Dependency",                        "EC", "EC_DEP", "事件自激性质",                       "event_A_followed_by_B_rate"},

    // ========== LR: LOB Resiliency (订单簿韧性) ==========
    {"rpl_vol",     "恢复量",              "Replenish Volume",                        "LR", "LR_VOL", "冲击后回补量",                       "replenish_vol = new_vol - old_vol"},
    {"rsl_rt",      "恢复率",              "Resiliency Ratio",                        "LR", "LR_RT",  "恢复量/冲击移除量",                  "replenish_vol / removed_vol"},
    {"rsl_spd",     "恢复速度",            "Resiliency Speed",                        "LR", "LR_SPD", "达到80%恢复所需时间",                "t_80%_recovery"},
    {"rsl_asym",    "恢复不对称",          "Resiliency Asymmetry",                    "LR", "LR_ASM", "买卖两侧恢复率差",                   "buy_recovery_rate - sell_recovery_rate"},
    {"rsl_dcy",     "韧性衰减",            "Resiliency Decay",                        "LR", "LR_DCY", "连续冲击下韧性演化",                 "resiliency_decay_curve"},

    // ========== AO: Abnormal Orders (异常挂单) ==========
    {"lmt_vol",     "涨跌停挂单量",        "Limit Price Volume",                      "AO", "AO_LMT", "涨跌停价附近挂单量",                 "vol_at_limit_price"},
    {"stale_vol",   "无成交大挂单",        "Stale Order Volume",                      "AO", "AO_STL", "长时间未成交的挂单量",               "stale_order_vol"},
    {"dur_vol",     "持续挂单量",          "Durable Order Volume",                    "AO", "AO_DUR", "某档维持x秒以上挂单量",              "durable_order_vol(x_sec)"},
    {"blk_vol",     "异常密集挂单",        "Blockage Volume",                         "AO", "AO_BLK", "偏离市价的集中大挂单",               "blockage_at_price(p)"},

    // ========== TI: Trade Initiation (逐笔主动交易) ==========
    {"trd_dir",     "交易方向",            "Trade Direction",                         "TI", "TI_DIR", "基于价格判断的交易方向",             "trade_dir = sign(price - mid)"},
    {"abuy_rt",     "主买比例",            "Active Buy Ratio",                        "TI", "TI_BR",  "时间窗口主动买成交量占比",           "active_buy_vol / total_vol"},
    {"net_flow",    "净流入",              "Net Inflow",                              "TI", "TI_NF",  "主买金额-主卖金额",                  "buy_amt - sell_amt"},
    {"labuy_rt",    "大单主买率",          "Large Active Buy Ratio",                  "TI", "TI_LBR", "大单主动买比例",                     "large_active_buy / large_total"},
    {"bs_swt",      "买卖切换率",          "Buy-Sell Switch Rate",                    "TI", "TI_SW",  "分时段主动方向变化率",               "buy_sell_switch_rate"},

    // ========== BS: Basic (基础特征) ==========
    {"spread",      "价差",                "Spread",                                  "BS", "BS_P",   "买一卖一价差",                       "spread = ask1 - bid1"},
    {"spread_z",    "价差z值",             "Spread Z-score",                          "BS", "BS_P",   "价差标准化",                         "z_score(spread)"},
    {"mid",         "中间价",              "Mid Price",                               "BS", "BS_P",   "买一卖一中间价",                     "mid = (bid1 + ask1) / 2"},
    {"tobi",        "顶层失衡",            "Top-of-book Imbalance",                   "BS", "BS_I",   "买一卖一量失衡",                     "(V_bid1 - V_ask1) / (V_bid1 + V_ask1)"},
    {"tobi_z",      "顶层失衡z值",         "Top-of-book Imbalance Z-score",           "BS", "BS_I",   "顶层失衡标准化",                     "z_score(tobi)"},
};
// clang-format on

#define TICK_SIZE 0.01f

// Convexity-weighted imbalance parameters
static constexpr int CWI_N = 3;
static constexpr float CWI_GAMMA[CWI_N] = {1.0f, 2.0f, 3.0f};

// Distance-discounted imbalance parameters
static constexpr int DDI_N = 3;
static constexpr float DDI_LAMBDAS[DDI_N] = {0.01f, 0.05f, 0.1f};

// Rolling Z-Score window size (30 minutes, assuming some interval)
static constexpr int ZSCORE_WINDOW = 1800;

class FeaturesTick {
public:
  //======================================================================================
  // CONSTRUCTOR
  //======================================================================================

  explicit FeaturesTick(const LOB_Feature *lob_feature)
      : lob_feature_(lob_feature) {}

  //======================================================================================
  // FEATURE CALCULATION
  //======================================================================================

  void update() {
    const auto &depth_buffer = lob_feature_->depth_buffer;

    // 检查 depth_buffer 是否有足够的数据
    if (depth_buffer.size() < 2 * LOB_FEATURE_DEPTH_LEVELS) {
      return; // 深度不足，跳过计算
    }

    // 提取 best bid/ask (depth_buffer 中间位置)
    // CBuffer: [0]:卖N, [N-1]:卖1, [N]:买1, ..., [2N-1]:买N
    const Level *best_ask_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS - 1];
    const Level *best_bid_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS];

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
        const Level *bid_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS + i];
        const Level *ask_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS - 1 - i];

        if (!bid_level || !ask_level)
          break;

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
        const Level *bid_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS + i];
        const Level *ask_level = depth_buffer[LOB_FEATURE_DEPTH_LEVELS - 1 - i];

        if (!bid_level || !ask_level)
          break;

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

private:
  const LOB_Feature *lob_feature_; // 指向 LOB_Feature，只读访问

  RollingZScore<float, ZSCORE_WINDOW> zs_spread_;
  RollingZScore<float, ZSCORE_WINDOW> zs_mpg_;
  RollingZScore<float, ZSCORE_WINDOW> zs_tobi_;
  RollingZScore<float, ZSCORE_WINDOW> zs_cwi_[CWI_N];
  RollingZScore<float, ZSCORE_WINDOW> zs_ddi_[DDI_N];

  float last_spread_z_ = 0.0f;
  float last_mpg_z_ = 0.0f;
  float last_tobi_z_ = 0.0f;
  float last_cwi_z_[CWI_N] = {};
  float last_ddi_z_[DDI_N] = {};
};
