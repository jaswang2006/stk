#pragma once

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits> // Required for std::is_same_v in macro expansion
#include <vector>

#include "./FeatureStoreConfig.hpp"

/*
事件驱动特征存储架构
========================================

核心设计：
- 事件驱动处理，带嵌套触发器级联机制
- 列式存储，配合父指针关联机制
- 零拷贝张量导出，对接ML流水线
- 完全基于宏的代码生成

扩展性：
--------------
添加 L4/L5/L6/L7 层级时，用户只需：
1. 在 FeatureStoreConfig.hpp 中定义 LEVEL_4_FIELDS(X), LEVEL_5_FIELDS(X) 等
2. 在 ALL_LEVELS(X) 宏中添加对应条目
3. 在 Features.hpp 中添加对应的触发器和更新器函数

以下代码全部由宏自动生成：
- init_L*_columns 初始化函数
- push_L*_fields 字段推送函数
- switch 语句处理不同层级
- 模板特化代码
- 张量导出逻辑
- 字段计数和大小计算

设计权衡：
------------------
性能 vs 灵活性：使用函数指针（快速）而非 std::function
内存 vs 访问：使用父指针（紧凑）而非数据复制
编译期 vs 运行期：完全宏生成，实现零运行时开销
类型安全 vs 性能：类型擦除以支持异构存储

可扩展性：
------------
- 行插入：O(1) 摊销复杂度
- 层级级联：O(LEVEL_COUNT)
- 内存增长：O(total_rows * fields_per_level)
- 导出：完全扩展 O(total_features * max_rows)
- 支持：1000+ 特征，单层 100M+ 行
*/

// =================================================================
// IMPLEMENTATION MACROS - AUTO-GENERATED, USER NEVER TOUCHES
// =================================================================

class ColumnStore;
class EventDrivenFeatureStore;

// Basic building block macros for implementation
#define ADD_COLUMN(name, type, comment) store.add_column<type>();
#define PUSH_FIELD(name, type, comment) store.push_field(col++, data.name);

// Auto-initialization call generation
#define GENERATE_INIT_CALL(level_name, level_num, fields, struct_name) \
  init_##level_name##_columns(stores_[level_num]);

#define GENERATE_ALL_INIT_CALLS() \
  ALL_LEVELS(GENERATE_INIT_CALL)

// Template specialization for push_row
#define GENERATE_PUSH_SPECIALIZATION(level_name, level_num, fields, struct_name) \
  if constexpr (std::is_same_v<LevelStruct, struct_name>) {                      \
    push_##level_name##_fields(stores_[level], row_data);                        \
  } else

#define GENERATE_ALL_PUSH_SPECIALIZATIONS() \
  ALL_LEVELS(GENERATE_PUSH_SPECIALIZATION) { /* empty else clause */ }

// Field counting for tensor export
#define COUNT_FIELDS(name, type, comment) +1
#define GET_FIELD_COUNT(level_name, level_num, fields, struct_name) \
  constexpr size_t level_name##_FIELD_COUNT = 0 fields(COUNT_FIELDS);
ALL_LEVELS(GET_FIELD_COUNT)

#define ADD_FIELD_COUNT(level_name, level_num, fields, struct_name) +level_name##_FIELD_COUNT
#define TOTAL_FIELD_COUNT (0 ALL_LEVELS(ADD_FIELD_COUNT))

// Tensor export field copying
#define COPY_FIELD_TO_EXPANDED_TENSOR(name, type, comment)                          \
  {                                                                                 \
    const auto &src_col = source_store.column(src_col_idx++);                       \
    void *expanded_col = std::aligned_alloc(64, tensor.max_rows * sizeof(type));    \
    expand_column_via_parents(level, 0, src_col, expanded_col, tensor.max_rows);    \
    tensor.column_ptrs[tensor_col_idx] = expanded_col;                              \
    tensor.element_sizes[tensor_col_idx] = sizeof(type);                            \
    tensor.feature_names[tensor_col_idx] = "L" + std::to_string(level) + "_" #name; \
    tensor_col_idx++;                                                               \
  }

#define GENERATE_TENSOR_EXPORT_CASE(level_name, level_num, fields, struct_name) \
  case level_name##_INDEX: {                                                    \
    size_t src_col_idx = 0;                                                     \
    fields(COPY_FIELD_TO_EXPANDED_TENSOR) break;                                \
  }

#define GENERATE_ALL_TENSOR_EXPORT_CASES()  \
  switch (level) {                          \
    ALL_LEVELS(GENERATE_TENSOR_EXPORT_CASE) \
  default:                                  \
    break;                                  \
  }

// Level 0 direct copy (no parent expansion)
#define COPY_L0_FIELD_DIRECT(name, type, comment)                                \
  {                                                                              \
    const auto &src_col = stores_[L0_INDEX].column(l0_col_idx++);                \
    void *expanded_col = std::aligned_alloc(64, tensor.max_rows * sizeof(type)); \
    std::memcpy(expanded_col, src_col.data(), src_col.size() * sizeof(type));    \
    tensor.column_ptrs[tensor_col_idx] = expanded_col;                           \
    tensor.element_sizes[tensor_col_idx] = sizeof(type);                         \
    tensor.feature_names[tensor_col_idx] = "L0_" #name;                          \
    tensor_col_idx++;                                                            \
  }

// Forward declarations for auto-generated functions
#define DECLARE_LEVEL_FUNCTIONS(level_name, level_num, fields, struct_name) \
  inline void init_##level_name##_columns(ColumnStore &store);              \
  inline void push_##level_name##_fields(ColumnStore &store, const struct_name &data);
ALL_LEVELS(DECLARE_LEVEL_FUNCTIONS)

// Type-erased column storage for heterogeneous data types
class TypeErasedColumn {
  void *data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
  size_t element_size_ = 0;

  void (*destroy_func_)(void *) = nullptr;
  void (*resize_func_)(void *&, size_t &, size_t, size_t) = nullptr;

public:
  TypeErasedColumn() = default;

  template <typename T>
  static std::unique_ptr<TypeErasedColumn> create(size_t initial_capacity = 1024) {
    auto col = std::make_unique<TypeErasedColumn>();
    col->initialize_for_type<T>(initial_capacity);
    return col;
  }

  ~TypeErasedColumn() {
    if (destroy_func_)
      destroy_func_(data_);
  }

  TypeErasedColumn(const TypeErasedColumn &) = delete;
  TypeErasedColumn &operator=(const TypeErasedColumn &) = delete;
  TypeErasedColumn(TypeErasedColumn &&) = default;
  TypeErasedColumn &operator=(TypeErasedColumn &&) = default;

  inline void push_back(const void *element) {
    if (size_ >= capacity_) [[unlikely]] {
      reserve(capacity_ == 0 ? 1024 : capacity_ * 2);
    }
    std::memcpy(static_cast<char *>(data_) + size_ * element_size_, element, element_size_);
    ++size_;
  }

  template <typename T>
  inline void push_back(const T &value) {
    push_back(static_cast<const void *>(&value));
  }

  void reserve(size_t new_capacity) {
    if (new_capacity > capacity_) {
      resize_func_(data_, capacity_, new_capacity, element_size_);
    }
  }

  size_t size() const { return size_; }
  void *data() { return data_; }
  const void *data() const { return data_; }
  size_t element_size() const { return element_size_; }

  template <typename T>
  T *typed_data() { return static_cast<T *>(data_); }
  template <typename T>
  const T *typed_data() const { return static_cast<const T *>(data_); }

private:
  template <typename T>
  void initialize_for_type(size_t initial_capacity) {
    element_size_ = sizeof(T);
    destroy_func_ = [](void *ptr) { std::free(ptr); };
    resize_func_ = [](void *&data, size_t &capacity, size_t new_cap, size_t elem_size) {
      void *new_data = std::aligned_alloc(alignof(T), new_cap * elem_size);
      if (data) {
        std::memcpy(new_data, data, capacity * elem_size);
        std::free(data);
      }
      data = new_data;
      capacity = new_cap;
    };
    reserve(initial_capacity);
  }
};

// Columnar data store - each field is stored as a separate column
class ColumnStore {
  std::vector<std::unique_ptr<TypeErasedColumn>> columns_;
  size_t row_count_ = 0;

public:
  ColumnStore() = default;

  template <typename T>
  size_t add_column(size_t initial_capacity = 1024) {
    columns_.push_back(TypeErasedColumn::create<T>(initial_capacity));
    return columns_.size() - 1;
  }

  template <typename T>
  inline void push_field(size_t column_idx, const T &value) {
    columns_[column_idx]->push_back(value);
  }

  inline void finish_row() {
    ++row_count_;
  }

  void reserve_all(size_t capacity) {
    for (auto &col : columns_) {
      col->reserve(capacity);
    }
  }

  size_t size() const { return row_count_; }
  size_t column_count() const { return columns_.size(); }

  TypeErasedColumn &column(size_t i) { return *columns_[i]; }
  const TypeErasedColumn &column(size_t i) const { return *columns_[i]; }

  template <typename T>
  T *column_data(size_t column_idx) {
    return columns_[column_idx]->template typed_data<T>();
  }

  template <typename T>
  const T *column_data(size_t column_idx) const {
    return columns_[column_idx]->template typed_data<T>();
  }
};

// Event-driven feature store with nested trigger mechanism
class EventDrivenFeatureStore {
private:
  // Columnar storage for each level
  std::array<ColumnStore, LEVEL_COUNT> stores_;

  // Parent row indices for nested pointer mechanism
  std::array<std::vector<size_t>, LEVEL_COUNT> parent_indices_;

  // High-performance trigger functions - function pointers for inline optimization
  bool (*triggers_[LEVEL_COUNT])() = {nullptr};
  void (*updaters_[LEVEL_COUNT])(EventDrivenFeatureStore &, size_t) = {nullptr};

  // Current latest row index for each level
  std::array<size_t, LEVEL_COUNT> current_row_indices_;

public:
  EventDrivenFeatureStore() {
    for (size_t i = 0; i < LEVEL_COUNT; ++i) {
      current_row_indices_[i] = 0;
    }
    // AUTO-INITIALIZE all levels at construction
    initialize_all_levels();
  }

  // Core event processing - nested trigger mechanism with inline optimization
  inline void on_event() {
    // Check triggers from highest level down to level 0
    for (int level = LEVEL_COUNT - 1; level >= 0; --level) {
      if (triggers_[level] && triggers_[level]()) {
        cascade_update(level);
        break;
      }
    }
  }

  // Set high-performance trigger function (function pointer for inline)
  void set_trigger(size_t level, bool (*trigger)()) {
    if (level < LEVEL_COUNT) {
      triggers_[level] = trigger;
    }
  }

  // Set high-performance updater function (function pointer for inline)
  void set_updater(size_t level, void (*updater)(EventDrivenFeatureStore &, size_t)) {
    if (level < LEVEL_COUNT) {
      updaters_[level] = updater;
    }
  }

  // AUTO-INITIALIZE all levels - MACRO-GENERATED
  void initialize_all_levels() {
    GENERATE_ALL_INIT_CALLS()
  }

  // Push a complete row to a level - AUTO-GENERATED template specializations
  template <typename LevelStruct>
  inline void push_row(size_t level, const LevelStruct &row_data, size_t parent_row_idx = 0) {
    if (level >= LEVEL_COUNT)
      return;

    // MACRO-GENERATED: Handles all current and future level types automatically
    GENERATE_ALL_PUSH_SPECIALIZATIONS()

    parent_indices_[level].push_back(parent_row_idx);
    stores_[level].finish_row();
    current_row_indices_[level] = stores_[level].size() - 1;
  }

  // Access interfaces
  ColumnStore &get_store(size_t level) { return stores_[level]; }
  const ColumnStore &get_store(size_t level) const { return stores_[level]; }

  size_t get_row_count(size_t level) const {
    return level < LEVEL_COUNT ? stores_[level].size() : 0;
  }

  size_t get_current_row_index(size_t level) const {
    return level < LEVEL_COUNT ? current_row_indices_[level] : 0;
  }

  size_t get_parent_row_index(size_t level, size_t row_idx) const {
    if (level == 0 || level >= LEVEL_COUNT || row_idx >= parent_indices_[level].size()) {
      return 0;
    }
    return parent_indices_[level][row_idx];
  }

  // Expanded tensor structure for ML training
  struct ExpandedTensor {
    // Flattened feature matrix: [total_features x max_rows]
    std::vector<void *> column_ptrs;        // Pointers to each feature column
    std::vector<size_t> element_sizes;      // Size of each element type
    std::vector<std::string> feature_names; // Feature names for debugging
    size_t total_features = 0;              // Total number of features across all levels
    size_t max_rows = 0;                    // Maximum rows (level 0 row count)

    template <typename T>
    T *get_feature_column(size_t feature_idx) {
      return static_cast<T *>(column_ptrs[feature_idx]);
    }
  };

  // Export to expanded tensor - MACRO-GENERATED for all levels
  ExpandedTensor to_expanded_tensor() const {
    ExpandedTensor tensor;

    // Get maximum rows (from level 0)
    tensor.max_rows = stores_[L0_INDEX].size();
    if (tensor.max_rows == 0)
      return tensor;

    // MACRO-GENERATED: Calculate total features across all levels
    tensor.total_features = TOTAL_FIELD_COUNT;

    // Allocate expanded columns
    tensor.column_ptrs.resize(tensor.total_features);
    tensor.element_sizes.resize(tensor.total_features);
    tensor.feature_names.resize(tensor.total_features);

    size_t tensor_col_idx = 0;

    // Level 0: Direct copy (no parent expansion needed)
    size_t l0_col_idx = 0;
    LEVEL_0_FIELDS(COPY_L0_FIELD_DIRECT)

    // Higher levels: Expand via parent pointers - MACRO-GENERATED
    for (size_t level = 1; level < LEVEL_COUNT; ++level) {
      const auto &source_store = stores_[level];
      if (source_store.size() == 0)
        continue;

      // MACRO-GENERATED: Handles all current and future levels automatically
      GENERATE_ALL_TENSOR_EXPORT_CASES()
    }

    return tensor;
  }

  // Cleanup expanded tensor memory
  static void free_expanded_tensor(ExpandedTensor &tensor) {
    for (void *ptr : tensor.column_ptrs) {
      if (ptr)
        std::free(ptr);
    }
    tensor.column_ptrs.clear();
    tensor.element_sizes.clear();
    tensor.feature_names.clear();
  }

private:
  // Cascade update implementation with inline optimization
  inline void cascade_update(size_t trigger_level) {
    for (size_t level = 0; level <= trigger_level; ++level) {
      if (updaters_[level]) {
        size_t parent_row_idx = (level == 0) ? 0 : current_row_indices_[level - 1];
        updaters_[level](*this, parent_row_idx);
      }
    }
  }

  // Expand higher-level column to level 0 size by following parent pointers
  void expand_column_via_parents(size_t level, size_t,
                                 const TypeErasedColumn &source_col,
                                 void *dest_buffer, size_t dest_rows) const {
    size_t elem_size = source_col.element_size();
    const uint8_t *source_data = static_cast<const uint8_t *>(source_col.data());
    uint8_t *dest_data = static_cast<uint8_t *>(dest_buffer);

    // For each level 0 row, find corresponding higher-level value
    for (size_t l0_row = 0; l0_row < dest_rows; ++l0_row) {
      size_t higher_level_row = find_corresponding_higher_level_row(level, l0_row);

      if (higher_level_row < source_col.size()) {
        std::memcpy(dest_data + l0_row * elem_size,
                    source_data + higher_level_row * elem_size,
                    elem_size);
      } else {
        // Fill with zeros if no corresponding row found
        std::memset(dest_data + l0_row * elem_size, 0, elem_size);
      }
    }
  }

  // Find corresponding higher-level row for a given level 0 row
  size_t find_corresponding_higher_level_row(size_t target_level, size_t l0_row) const {
    if (target_level == 0)
      return l0_row;

    // Walk up the parent chain to find the corresponding row at target_level
    size_t current_row = l0_row;

    for (size_t level = 1; level <= target_level; ++level) {
      // Find the most recent row in 'level' that has parent_row <= current_row
      size_t best_row = 0;
      for (size_t row = 0; row < parent_indices_[level].size(); ++row) {
        if (parent_indices_[level][row] <= current_row) {
          best_row = row;
        } else {
          break;
        }
      }
      current_row = best_row;
    }

    return current_row;
  }
};

// =================================================================
// AUTO-GENERATED FUNCTIONS - AUTOMATICALLY SCALES TO ALL LEVELS
// =================================================================

// MACRO-GENERATED: Column initialization functions for all levels
#define DEFINE_INIT_FUNCTION(level_name, level_num, fields, struct_name) \
  inline void init_##level_name##_columns(ColumnStore &store) {          \
    fields(ADD_COLUMN)                                                   \
  }
ALL_LEVELS(DEFINE_INIT_FUNCTION)

// MACRO-GENERATED: Field pushing functions for all levels
#define DEFINE_PUSH_FUNCTION(level_name, level_num, fields, struct_name)                \
  inline void push_##level_name##_fields(ColumnStore &store, const struct_name &data) { \
    size_t col = 0;                                                                     \
    fields(PUSH_FIELD)                                                                  \
  }
ALL_LEVELS(DEFINE_PUSH_FUNCTION)

// Global instance
inline EventDrivenFeatureStore &get_feature_store() {
  static EventDrivenFeatureStore instance;
  return instance;
}
