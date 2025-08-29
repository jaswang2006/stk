#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <unordered_set>

namespace L2 {
namespace Parallel {

/**
 * Task structure for encoding work distributed to worker threads
 */
struct EncodingTask {
    std::string asset_dir;      // Full path to asset directory (e.g., temp/20170103/600000.SH)
    std::string asset_code;     // Asset code (e.g., 600000.SH)
    std::string date_str;       // Date string (e.g., 20170103)
    std::string output_base;    // Base output directory
};

/**
 * Thread-safe task queue for distributing encoding work
 */
class TaskQueue {
private:
    std::queue<EncodingTask> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> finished_{false};

public:
    void push(const EncodingTask& task);
    bool pop(EncodingTask& task);
    void finish();
    size_t size() const;
};

/**
 * Multi-buffer state coordination for decompression and encoding pipeline
 * Manages multiple temporary directories to overlap decompression and encoding
 * Supports configurable number of decompression threads and buffers
 */
class MultiBufferState {
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> decompression_finished_{false};
    
    // Buffer management
    std::vector<std::string> buffer_dirs_;           // All buffer directory paths
    std::unordered_set<size_t> ready_buffers_;       // Buffers ready for encoding
    std::unordered_set<size_t> in_use_buffers_;      // Buffers currently being used by encoders
    std::unordered_set<size_t> available_buffers_;   // Buffers available for decompression
    
public:
    explicit MultiBufferState(const std::string& temp_base, uint32_t num_buffers);
    ~MultiBufferState();
    
    // Decompressor calls this to get next available directory for decompression
    std::string get_available_decomp_dir();
    
    // Decompressor calls this when a directory is ready for encoding
    void signal_ready(const std::string& dir);
    
    // Encoders call this to get next available directory for encoding
    std::string wait_for_ready_dir();
    
    // Encoders call this when done with a directory
    void finish_with_dir(const std::string& dir);
    
    // Decompressor calls this when all decompression is finished
    void signal_decompression_finished();
    
    // Get buffer directory count
    size_t get_buffer_count() const { return buffer_dirs_.size(); }
    
private:
    size_t find_buffer_index(const std::string& dir) const;
};

} // namespace Parallel
} // namespace L2
