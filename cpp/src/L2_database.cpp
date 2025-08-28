// L2 Database Parallel Processing Pipeline
// Processes compressed 7z archives containing daily stock data
// Uses ping-pong decompression with multi-threaded encoding

#include "codec/parallel/processing_types.hpp"
#include "codec/parallel/workers.hpp"
#include "misc/affinity.hpp"

// Standard library includes
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

/**
 * Main function implementing the parallel processing pipeline
 * Architecture:
 * 1. Core 0: Decompression worker
 * 2. Cores 1+: Encoding workers
 * 3. Ping-pong coordination for overlapped processing
 */
int main() {
    using namespace L2;
    using namespace L2::Parallel;
    
    // Configuration - hardcoded paths for production use
    const std::string input_base = "/mnt/dev/sde/A_stock/L2";  // Base directory for 7z files
    const std::string output_base = "/mnt/dev/sde/A_stock/L2_binary";  // Base output directory
    const std::string temp_base = "/tmp/L2_processing";  // Base temp directory
    
    std::cout << "L2 Database Processing Pipeline (Parallel)" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << "Input directory: " << input_base << std::endl;
    std::cout << "Output directory: " << output_base << std::endl;
    std::cout << "Temp directory: " << temp_base << std::endl;
    std::cout << std::endl;
    
    // Clean output and temp directories from previous runs
    std::cout << "Cleaning directories from previous runs..." << std::endl;
    if (std::filesystem::exists(output_base)) {
        std::filesystem::remove_all(output_base);
        std::cout << "Cleaned output directory: " << output_base << std::endl;
    }
    if (std::filesystem::exists(temp_base)) {
        std::filesystem::remove_all(temp_base);
        std::cout << "Cleaned temp directory: " << temp_base << std::endl;
    }
    std::cout << std::endl;
    
    // Setup ping-pong coordination and create directories
    PingPongState ping_pong(temp_base);
    std::filesystem::create_directories(output_base);
    
    // Determine CPU core allocation
    unsigned int num_cores = misc::Affinity::core_count();
    unsigned int encoding_threads = num_cores > 1 ? num_cores - 1 : 1;  // Reserve 1 core for decompression
    
    std::cout << "CPU cores available: " << num_cores << std::endl;
    std::cout << "Decompression threads: 1 (core 0)" << std::endl;
    std::cout << "Encoding threads: " << encoding_threads << " (cores 1-" << num_cores-1 << ")" << std::endl;
    std::cout << std::endl;
    
    // Collect all archive files to process
    std::vector<std::string> all_archives;
    for (int year = 2017; year <= 2024; ++year) {
        std::string year_dir = input_base + "/" + std::to_string(year);
        if (!std::filesystem::exists(year_dir)) {
            std::cout << "Year directory does not exist: " << year_dir << ", skipping..." << std::endl;
            continue;
        }
        
        for (int month = 1; month <= 12; ++month) {
            std::string month_str = (month < 10) ? "0" + std::to_string(month) : std::to_string(month);
            std::string month_dir = year_dir + "/" + month_str;
            
            if (!std::filesystem::exists(month_dir)) {
                continue;
            }
            
            // Find all 7z archives in this month
            for (const auto& entry : std::filesystem::directory_iterator(month_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".7z") {
                    all_archives.push_back(entry.path().string());
                }
            }
        }
    }
    
    std::sort(all_archives.begin(), all_archives.end());
    std::cout << "Found " << all_archives.size() << " archive files to process" << std::endl;
    std::cout << std::endl;
    
    // Initialize task queue and counters
    TaskQueue task_queue;
    std::atomic<int> completed_tasks{0};
    std::atomic<int> total_assets{0};
    
    // Start decompression worker thread on core 0
    std::thread decomp_worker(decompression_worker, 
                             std::cref(all_archives), 
                             std::ref(ping_pong), 
                             std::ref(task_queue),
                             std::cref(output_base),
                             std::ref(total_assets));
    
    // Start encoding worker threads on cores 1+
    std::vector<std::thread> workers;
    for (unsigned int i = 1; i <= encoding_threads; ++i) {
        workers.emplace_back(encoding_worker_with_pingpong, 
                           std::ref(task_queue), 
                           std::ref(ping_pong),
                           i, 
                           std::ref(completed_tasks));
    }
    
    // Wait for decompression to finish
    decomp_worker.join();
    
    // Signal encoding workers to finish and wait for completion
    task_queue.finish();
    for (auto& worker : workers) {
        worker.join();
    }
    
    // Cleanup temporary directories
    if (std::filesystem::exists(ping_pong.temp_dir_a_)) {
        std::filesystem::remove_all(ping_pong.temp_dir_a_);
    }
    if (std::filesystem::exists(ping_pong.temp_dir_b_)) {
        std::filesystem::remove_all(ping_pong.temp_dir_b_);
    }
    
    // Report final statistics
    std::cout << std::endl;
    std::cout << "=== PROCESSING COMPLETE ===" << std::endl;
    std::cout << "Total assets processed: " << completed_tasks.load() << "/" << total_assets.load() << std::endl;
    std::cout << "Output saved to: " << output_base << std::endl;
    
    return 0;
}
