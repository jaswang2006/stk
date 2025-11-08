#pragma once

#include "../FeaturesDefine.hpp"
#include <cstddef>

// ============================================================================
// FEATURE STORE CONFIGURATION - AUTO-GENERATED FROM SCHEMA
// ============================================================================
// Fully macro-driven code generation:
// - Level metadata (count, indices)
// - Field metadata (count, offsets)
// - Data structures (LevelNData structs)
// - Capacity configuration
//
// Storage format: float16 (IEEE 754 binary16, uint16_t representation)
// - Reduces memory/disk usage by 50% vs float32
// - Sufficient precision for normalized features (±65504, ~3 decimal digits)
// - CPU: convert via software (uint16_t <-> float32)
// - GPU: native fp16 support on modern hardware
// ============================================================================

// Storage and computation type: _Float16 (native compiler support)
// - Clang/GCC native type with automatic float <-> _Float16 conversion
// - Hardware accelerated on modern CPUs (F16C, AVX-512 FP16, ARM NEON)
// - Reduces memory/disk by 50% vs float32
// - Sufficient precision for normalized features (±65504, ~3.3 decimal digits)
using feature_storage_t = _Float16;

// ============================================================================
// LEVEL METADATA - AUTO-GENERATED
// ============================================================================

// Count total number of levels
#define COUNT_LEVEL(level_name, level_num, fields) +1
constexpr size_t LEVEL_COUNT = 0 ALL_LEVELS(COUNT_LEVEL);

// Generate level index constants: L0_INDEX, L1_INDEX, L2_INDEX, ...
#define GENERATE_LEVEL_INDEX(level_name, level_num, fields) \
  constexpr size_t level_name##_INDEX = level_num;
ALL_LEVELS(GENERATE_LEVEL_INDEX)

// ============================================================================
// FIELD METADATA - AUTO-GENERATED
// ============================================================================

// Count fields in each level
#define COUNT_FIELD(code, cn, en, dtype, c1, c2, norm, formula, desc) +1

#define GENERATE_FIELD_COUNT_FOR_LEVEL(level_name, level_num, fields) \
  constexpr size_t level_name##_FIELD_COUNT = 0 fields(COUNT_FIELD);
ALL_LEVELS(GENERATE_FIELD_COUNT_FOR_LEVEL)

// Generate field offset enums for each level (scoped to avoid name collisions)
#define GENERATE_OFFSET_ENUM_FOR_LEVEL(level_name, level_num, fields) \
  namespace level_name##_FieldOffset {                                \
    enum : size_t {                                                   \
      fields(GENERATE_FIELD_OFFSET_##level_name)                      \
    };                                                                \
  }

#define GENERATE_FIELD_OFFSET_L0(code, cn, en, dtype, c1, c2, norm, formula, desc) code,
#define GENERATE_FIELD_OFFSET_L1(code, cn, en, dtype, c1, c2, norm, formula, desc) code,
#define GENERATE_FIELD_OFFSET_L2(code, cn, en, dtype, c1, c2, norm, formula, desc) code,

ALL_LEVELS(GENERATE_OFFSET_ENUM_FOR_LEVEL)

// ============================================================================
// DATA STRUCTURES - AUTO-GENERATED
// ============================================================================

// Generate LevelNData structs with all fields as feature_storage_t (_Float16)
#define GENERATE_STRUCT_FIELD(code, cn, en, dtype, c1, c2, norm, formula, desc) feature_storage_t code;

#define GENERATE_LEVEL_DATA_STRUCT(level_name, level_num, fields) \
  struct Level##level_num##Data {                                 \
    fields(GENERATE_STRUCT_FIELD)                                 \
  };
ALL_LEVELS(GENERATE_LEVEL_DATA_STRUCT)

// ============================================================================
// FEATURE TYPE METADATA - AUTO-GENERATED
// ============================================================================

struct FieldTypeMeta {
  size_t offset;
  FeatureDataType type;
};

// Generate FieldTypeMeta entry from field definition
// Format: X(code, name_cn, name_en, data_type, cat_l1, cat_l2, norm, formula, desc)
#define GENERATE_FIELD_TYPE_META_L0(code, cn, en, dtype, c1, c2, norm, formula, desc) \
  {L0_FieldOffset::code, FeatureDataType::dtype},

#define GENERATE_FIELD_TYPE_META_L1(code, cn, en, dtype, c1, c2, norm, formula, desc) \
  {L1_FieldOffset::code, FeatureDataType::dtype},

#define GENERATE_FIELD_TYPE_META_L2(code, cn, en, dtype, c1, c2, norm, formula, desc) \
  {L2_FieldOffset::code, FeatureDataType::dtype},

// Generate FieldTypeMeta arrays for each level
inline constexpr FieldTypeMeta L0_FIELD_TYPES[] = {
  LEVEL_0_FIELDS(GENERATE_FIELD_TYPE_META_L0)
};

inline constexpr FieldTypeMeta L1_FIELD_TYPES[] = {
  LEVEL_1_FIELDS(GENERATE_FIELD_TYPE_META_L1)
};

inline constexpr FieldTypeMeta L2_FIELD_TYPES[] = {
  LEVEL_2_FIELDS(GENERATE_FIELD_TYPE_META_L2)
};

// ============================================================================
// CAPACITY AND FIELD CONFIGURATION
// ============================================================================

// Field counts per level
#define GENERATE_FIELD_COUNT_ENTRY(level_name, level_num, fields) \
  level_name##_FIELD_COUNT,
constexpr size_t FIELDS_PER_LEVEL[LEVEL_COUNT] = {
    ALL_LEVELS(GENERATE_FIELD_COUNT_ENTRY)};

// Max capacities per level (from LEVEL_CONFIGS)
constexpr size_t MAX_ROWS_PER_LEVEL[LEVEL_COUNT] = {
    LEVEL_CONFIGS[0].max_capacity(),
    LEVEL_CONFIGS[1].max_capacity(),
    LEVEL_CONFIGS[2].max_capacity(),
};

// ============================================================================
// FEATURE TYPE RANGES - For optimized TS/CS/LB/OT access
// ============================================================================
// Helper to get feature index ranges by type
// Usage: GET_FEATURE_RANGE(L0, TS) returns {start_idx, end_idx}

struct FeatureRange {
  size_t start;
  size_t end;
  size_t count() const { return end - start; }
};

// Compile-time computation of feature ranges per level
template<size_t Level>
constexpr FeatureRange get_feature_range(FeatureDataType type) {
  const FieldTypeMeta* types = nullptr;
  size_t count = 0;
  
  if constexpr (Level == 0) {
    types = L0_FIELD_TYPES;
    count = sizeof(L0_FIELD_TYPES) / sizeof(FieldTypeMeta);
  } else if constexpr (Level == 1) {
    types = L1_FIELD_TYPES;
    count = sizeof(L1_FIELD_TYPES) / sizeof(FieldTypeMeta);
  } else if constexpr (Level == 2) {
    types = L2_FIELD_TYPES;
    count = sizeof(L2_FIELD_TYPES) / sizeof(FieldTypeMeta);
  }
  
  size_t start = count;
  size_t end = 0;
  for (size_t i = 0; i < count; ++i) {
    if (types[i].type == type) {
      if (types[i].offset < start) start = types[i].offset;
      if (types[i].offset >= end) end = types[i].offset + 1;
    }
  }
  return {start, end};
}

// Macros for runtime level selection (prefer constexpr ranges above for compile-time)
#define GET_TS_RANGE(level_idx) get_feature_range<level_idx>(FeatureDataType::TS)
#define GET_CS_RANGE(level_idx) get_feature_range<level_idx>(FeatureDataType::CS)
#define GET_LB_RANGE(level_idx) get_feature_range<level_idx>(FeatureDataType::LB)
#define GET_SH_RANGE(level_idx) get_feature_range<level_idx>(FeatureDataType::SH)
#define GET_META_RANGE(level_idx) get_feature_range<level_idx>(FeatureDataType::META)

// ============================================================================
// FEATURE TYPE RANGES - Auto-computed from FeatureDataType, name-independent
// ============================================================================

// Auto-computed ranges for each level and data type
// Usage: L0_TS_RANGE.start, L0_TS_RANGE.end, L0_TS_RANGE.count()

// Level 0
constexpr auto L0_TS_RANGE = get_feature_range<0>(FeatureDataType::TS);
constexpr auto L0_CS_RANGE = get_feature_range<0>(FeatureDataType::CS);
constexpr auto L0_LB_RANGE = get_feature_range<0>(FeatureDataType::LB);
constexpr auto L0_SH_RANGE = get_feature_range<0>(FeatureDataType::SH);
constexpr auto L0_META_RANGE = get_feature_range<0>(FeatureDataType::META);

// Level 1
constexpr auto L1_TS_RANGE = get_feature_range<1>(FeatureDataType::TS);
constexpr auto L1_CS_RANGE = get_feature_range<1>(FeatureDataType::CS);
constexpr auto L1_LB_RANGE = get_feature_range<1>(FeatureDataType::LB);
constexpr auto L1_SH_RANGE = get_feature_range<1>(FeatureDataType::SH);

// Level 2
constexpr auto L2_TS_RANGE = get_feature_range<2>(FeatureDataType::TS);
constexpr auto L2_CS_RANGE = get_feature_range<2>(FeatureDataType::CS);
constexpr auto L2_LB_RANGE = get_feature_range<2>(FeatureDataType::LB);
constexpr auto L2_SH_RANGE = get_feature_range<2>(FeatureDataType::SH);

// Legacy aliases for compatibility (deprecated, use L0_TS_RANGE.start/.end instead)
constexpr size_t L0_TS_START = L0_TS_RANGE.start;
constexpr size_t L0_TS_END = L0_TS_RANGE.end;
constexpr size_t L0_CS_START = L0_CS_RANGE.start;
constexpr size_t L0_CS_END = L0_CS_RANGE.end;
constexpr size_t L0_LB_START = L0_LB_RANGE.start;
constexpr size_t L0_LB_END = L0_LB_RANGE.end;
constexpr size_t L0_SH_START = L0_SH_RANGE.start;
constexpr size_t L0_SH_END = L0_SH_RANGE.end;
constexpr size_t L0_META_START = L0_META_RANGE.start;

// ============================================================================
// TIME INDEX CONVERSION
// ============================================================================

inline size_t time_to_index(size_t level_idx, uint8_t hour, uint8_t minute, uint8_t second, uint8_t millisecond) {
  const LevelTimeConfig &cfg = LEVEL_CONFIGS[level_idx];

  switch (cfg.unit) {
  case TimeUnit::MILLISECOND: {
    const size_t ms = time_to_trading_milliseconds(hour, minute, second, millisecond);
    return ms / cfg.interval;
  }
  case TimeUnit::SECOND: {
    const size_t sec = time_to_trading_seconds(hour, minute, second);
    return sec / cfg.interval;
  }
  case TimeUnit::MINUTE: {
    const size_t sec = time_to_trading_seconds(hour, minute, second);
    return (sec / 60) / cfg.interval;
  }
  case TimeUnit::HOUR: {
    const size_t sec = time_to_trading_seconds(hour, minute, second);
    return (sec / 3600) / cfg.interval;
  }
  }
  return 0;
}
