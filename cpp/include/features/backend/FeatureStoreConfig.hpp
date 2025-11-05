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
// All features stored as float (normalized distribution assumption)
// ============================================================================

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
#define COUNT_FIELD(name, comment) +1

#define GENERATE_FIELD_COUNT_FOR_LEVEL(level_name, level_num, fields) \
  constexpr size_t level_name##_FIELD_COUNT = 0 fields(COUNT_FIELD);
ALL_LEVELS(GENERATE_FIELD_COUNT_FOR_LEVEL)

// Generate field offset enums for each level (scoped to avoid name collisions)
#define GENERATE_OFFSET_ENUM_FOR_LEVEL(level_name, level_num, fields)     \
  namespace level_name##_FieldOffset {                                     \
    enum : size_t {                                                        \
      fields(GENERATE_FIELD_OFFSET_##level_name)                           \
    };                                                                     \
  }

#define GENERATE_FIELD_OFFSET_L0(name, comment) name,
#define GENERATE_FIELD_OFFSET_L1(name, comment) name,
#define GENERATE_FIELD_OFFSET_L2(name, comment) name,

ALL_LEVELS(GENERATE_OFFSET_ENUM_FOR_LEVEL)

// ============================================================================
// DATA STRUCTURES - AUTO-GENERATED
// ============================================================================

// Generate LevelNData structs with all fields as float
#define GENERATE_STRUCT_FIELD(name, comment) float name;

#define GENERATE_LEVEL_DATA_STRUCT(level_name, level_num, fields) \
  struct Level##level_num##Data {                                  \
    fields(GENERATE_STRUCT_FIELD)                                  \
  };
ALL_LEVELS(GENERATE_LEVEL_DATA_STRUCT)

// ============================================================================
// CAPACITY CONFIGURATION
// ============================================================================

// Default row capacities per asset per day per level
#ifndef MAX_L0_ROWS_PER_ASSET_PER_DAY
  constexpr size_t MAX_L0_ROWS_PER_ASSET_PER_DAY = 1'000'000;  // 1M ticks
#endif

#ifndef MAX_L1_ROWS_PER_ASSET_PER_DAY
  constexpr size_t MAX_L1_ROWS_PER_ASSET_PER_DAY = 10'000;     // 10K minutes
#endif

#ifndef MAX_L2_ROWS_PER_ASSET_PER_DAY
  constexpr size_t MAX_L2_ROWS_PER_ASSET_PER_DAY = 1'000;      // 1K hours
#endif

// Aggregate into array for runtime access
#define GENERATE_CAPACITY_ENTRY(level_name, level_num, fields) \
  MAX_##level_name##_ROWS_PER_ASSET_PER_DAY,
constexpr size_t MAX_ROWS_PER_LEVEL[LEVEL_COUNT] = {
  ALL_LEVELS(GENERATE_CAPACITY_ENTRY)
};

// Aggregate field counts into array for runtime access
#define GENERATE_FIELD_COUNT_ENTRY(level_name, level_num, fields) \
  level_name##_FIELD_COUNT,
constexpr size_t FIELDS_PER_LEVEL[LEVEL_COUNT] = {
  ALL_LEVELS(GENERATE_FIELD_COUNT_ENTRY)
};
