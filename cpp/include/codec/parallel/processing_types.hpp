#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

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
 * Ping-pong state coordination for decompression and encoding pipeline
 * Manages two temporary directories in ping-pong fashion to overlap decompression and encoding
 */
class PingPongState {
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> decompression_finished_{false};
    
public:
    // Ping-pong directories
    std::string temp_dir_a_;
    std::string temp_dir_b_;
    std::atomic<bool> a_is_ready_{false};   // true when temp_a has data ready for encoding
    std::atomic<bool> b_is_ready_{false};   // true when temp_b has data ready for encoding
    std::atomic<bool> a_in_use_{false};     // true when encoders are working on temp_a
    std::atomic<bool> b_in_use_{false};     // true when encoders are working on temp_b
    
    explicit PingPongState(const std::string& temp_base);
    
    // Decompressor calls this when a directory is ready
    void signal_ready(bool is_dir_a);
    
    // Encoders call this to get next available directory
    std::string wait_for_ready_dir();
    
    // Encoders call this when done with a directory
    void finish_with_dir(const std::string& dir);
    
    // Decompressor calls this to get next available directory for decompression
    std::string get_available_decomp_dir();
    
    void signal_decompression_finished();
};

} // namespace Parallel
} // namespace L2
