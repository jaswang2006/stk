#include "codec/parallel/processing_types.hpp"
#include <chrono>
#include <thread>
#include <filesystem>

namespace L2 {
namespace Parallel {

// TaskQueue implementation
void TaskQueue::push(const EncodingTask& task) {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push(task);
    cv_.notify_one();
}

bool TaskQueue::pop(EncodingTask& task) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !tasks_.empty() || finished_.load(); });
    
    if (tasks_.empty()) {
        return false;
    }
    
    task = tasks_.front();
    tasks_.pop();
    return true;
}

void TaskQueue::finish() {
    finished_.store(true);
    cv_.notify_all();
}

size_t TaskQueue::size() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return tasks_.size();
}

// PingPongState implementation
PingPongState::PingPongState(const std::string& temp_base) 
    : temp_dir_a_(temp_base + "/temp_a"), temp_dir_b_(temp_base + "/temp_b") {
    std::filesystem::create_directories(temp_dir_a_);
    std::filesystem::create_directories(temp_dir_b_);
}

void PingPongState::signal_ready(bool is_dir_a) {
    if (is_dir_a) {
        a_is_ready_.store(true);
    } else {
        b_is_ready_.store(true);
    }
    cv_.notify_all();
}

std::string PingPongState::wait_for_ready_dir() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { 
        return (a_is_ready_.load() && !a_in_use_.load()) || 
               (b_is_ready_.load() && !b_in_use_.load()) || 
               decompression_finished_.load(); 
    });
    
    if (a_is_ready_.load() && !a_in_use_.load()) {
        a_in_use_.store(true);
        return temp_dir_a_;
    } else if (b_is_ready_.load() && !b_in_use_.load()) {
        b_in_use_.store(true);
        return temp_dir_b_;
    }
    
    return "";  // Decompression finished and no more work
}

void PingPongState::finish_with_dir(const std::string& dir) {
    if (dir == temp_dir_a_) {
        a_is_ready_.store(false);
        a_in_use_.store(false);
    } else if (dir == temp_dir_b_) {
        b_is_ready_.store(false);
        b_in_use_.store(false);
    }
    cv_.notify_all();
}

std::string PingPongState::get_available_decomp_dir() {
    // Wait for a directory to be free
    while (true) {
        if (!a_is_ready_.load() && !a_in_use_.load()) {
            return temp_dir_a_;
        }
        if (!b_is_ready_.load() && !b_in_use_.load()) {
            return temp_dir_b_;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void PingPongState::signal_decompression_finished() {
    decompression_finished_.store(true);
    cv_.notify_all();
}

} // namespace Parallel
} // namespace L2
