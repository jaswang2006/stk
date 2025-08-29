// L2 Database Parallel Processing Pipeline
// Processes compressed 7z archives containing daily stock data
// Uses configurable multi-buffer decompression with multi-threaded encoding

#include "codec/parallel/processing_types.hpp"
#include "codec/parallel/workers.hpp"
#include "codec/parallel/processing_config.hpp"
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
 * 1. Multiple decompression workers (configurable)
 * 2. Multiple encoding workers (configurable)
 * 3. Multi-buffer coordination for overlapped processing
 */
int main() {
    using namespace L2;
    using namespace L2::Parallel;
    
    // Determine CPU core count and auto-configure
    unsigned int num_cores = misc::Affinity::core_count();
    g_config.auto_configure(num_cores);
    
    // Validate configuration
    if (!g_config.validate()) {
        std::cerr << "FATAL ERROR: Invalid configuration parameters!" << std::endl;
        return 1;
    }
    
    std::cout << "L2 Database Processing Pipeline (Multi-Buffer)" << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << "Input directory: " << g_config.input_base << std::endl;
    std::cout << "Output directory: " << g_config.output_base << std::endl;
    std::cout << "Temp directory: " << g_config.temp_base << std::endl;
    std::cout << "CPU cores available: " << num_cores << std::endl;
    std::cout << "Decompression threads: " << g_config.decompression_threads << std::endl;
    std::cout << "Decompression buffers: " << g_config.decompression_buffers << std::endl;
    std::cout << "Encoding threads: " << g_config.encoding_threads << std::endl;
    std::cout << "Terminate on error: " << (g_config.terminate_on_error ? "YES" : "NO") << std::endl;
    std::cout << std::endl;
    
    // Clean output and temp directories from previous runs
    std::cout << "Cleaning directories from previous runs..." << std::endl;
    if (std::filesystem::exists(g_config.output_base)) {
        std::filesystem::remove_all(g_config.output_base);
        std::cout << "Cleaned output directory: " << g_config.output_base << std::endl;
    }
    if (std::filesystem::exists(g_config.temp_base)) {
        std::filesystem::remove_all(g_config.temp_base);
        std::cout << "Cleaned temp directory: " << g_config.temp_base << std::endl;
    }
    std::cout << std::endl;
    
    // Setup multi-buffer coordination and create directories
    MultiBufferState multi_buffer(g_config.temp_base, g_config.decompression_buffers);
    std::filesystem::create_directories(g_config.output_base);
    
    // Collect all archive files to process
    std::vector<std::string> all_archives;
    for (int year = 2017; year <= 2024; ++year) {
        std::string year_dir = std::string(g_config.input_base) + "/" + std::to_string(year);
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
    
    // Distribute archives among decompression workers
    std::vector<std::vector<std::string>> archive_subsets(g_config.decompression_threads);
    for (size_t i = 0; i < all_archives.size(); ++i) {
        archive_subsets[i % g_config.decompression_threads].push_back(all_archives[i]);
    }
    
    // Start decompression worker threads
    std::vector<std::thread> decomp_workers;
    for (unsigned int i = 0; i < g_config.decompression_threads; ++i) {
        if (!archive_subsets[i].empty()) {
            decomp_workers.emplace_back(decompression_worker, 
                                       std::cref(archive_subsets[i]), 
                                       std::ref(multi_buffer), 
                                       std::ref(task_queue),
                                       std::string(g_config.output_base),
                                       std::ref(total_assets),
                                       i);
        }
    }
    
    // Start encoding worker threads
    std::vector<std::thread> encoding_workers;
    for (unsigned int i = 1; i <= g_config.encoding_threads; ++i) {
        encoding_workers.emplace_back(encoding_worker_with_multibuffer, 
                                     std::ref(task_queue), 
                                     std::ref(multi_buffer),
                                     i, 
                                     std::ref(completed_tasks));
    }
    
    // Wait for all decompression workers to finish
    for (auto& worker : decomp_workers) {
        worker.join();
    }
    
    // Signal decompression finished
    multi_buffer.signal_decompression_finished();
    
    // Signal encoding workers to finish and wait for completion
    task_queue.finish();
    for (auto& worker : encoding_workers) {
        worker.join();
    }
    
    // Cleanup is handled automatically by MultiBufferState destructor
    
    // Report final statistics
    std::cout << std::endl;
    std::cout << "=== PROCESSING COMPLETE ===" << std::endl;
    std::cout << "Total assets processed: " << completed_tasks.load() << "/" << total_assets.load() << std::endl;
    std::cout << "Output saved to: " << g_config.output_base << std::endl;
    
    return 0;
}
