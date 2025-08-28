#include "codec/parallel/workers.hpp"
#include "codec/binary_encoder_L2.hpp"
#include "misc/affinity.hpp"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cassert>
#include <cstring>

namespace L2 {
namespace Parallel {

bool process_stock_data(const std::string& asset_dir, 
                       const std::string& asset_code,
                       const std::string& date_str,
                       const std::string& output_base) {
    // Parse date components for directory structure
    assert(date_str.length() == 8);
    std::string year = date_str.substr(0, 4);
    std::string month = date_str.substr(4, 2);
    std::string day = date_str.substr(6, 2);
    
    // Create output directory structure: /output_base/year/month/day/asset_code/
    std::string output_dir = output_base + "/" + year + "/" + month + "/" + day + "/" + asset_code;
    std::filesystem::create_directories(output_dir);
    
    // Create encoder instance
    L2::BinaryEncoder_L2 encoder(200000, 1000000);
    
    // Check if asset directory exists
    if (!std::filesystem::exists(asset_dir)) {
        std::cerr << "Asset directory does not exist: " << asset_dir << std::endl;
        return false;
    }
    
    std::vector<L2::Snapshot> snapshots;
    std::vector<L2::Order> all_orders;
    bool has_snapshots = false;
    bool has_orders = false;
    
    // Process snapshots from 行情.csv
    std::string snapshot_file = asset_dir + "/行情.csv";
    if (std::filesystem::exists(snapshot_file)) {
        std::vector<L2::CSVSnapshot> csv_snapshots;
        if (encoder.parse_snapshot_csv(snapshot_file, csv_snapshots)) {
            snapshots.reserve(csv_snapshots.size());
            for (const auto& csv_snap : csv_snapshots) {
                snapshots.push_back(L2::BinaryEncoder_L2::csv_to_snapshot(csv_snap));
            }
            has_snapshots = !snapshots.empty();
        }
    }
    
    // Process orders from 委托队列.csv
    std::string order_file = asset_dir + "/委托队列.csv";
    if (std::filesystem::exists(order_file)) {
        std::vector<L2::CSVOrder> csv_orders;
        if (encoder.parse_order_csv(order_file, csv_orders)) {
            all_orders.reserve(csv_orders.size());
            for (const auto& csv_order : csv_orders) {
                all_orders.push_back(L2::BinaryEncoder_L2::csv_to_order(csv_order));
            }
        }
    }
    
    // Process trades from 逐笔成交.csv
    std::string trade_file = asset_dir + "/逐笔成交.csv";
    if (std::filesystem::exists(trade_file)) {
        std::vector<L2::CSVTrade> csv_trades;
        if (encoder.parse_trade_csv(trade_file, csv_trades)) {
            size_t original_size = all_orders.size();
            all_orders.reserve(original_size + csv_trades.size());
            for (const auto& csv_trade : csv_trades) {
                all_orders.push_back(L2::BinaryEncoder_L2::csv_to_trade(csv_trade));
            }
        }
    }
    
    has_orders = !all_orders.empty();
    
    // Sort orders by time if needed
    if (has_orders) {
        std::sort(all_orders.begin(), all_orders.end(), [](const L2::Order& a, const L2::Order& b) {
            if (a.hour != b.hour) return a.hour < b.hour;
            if (a.minute != b.minute) return a.minute < b.minute;
            if (a.second != b.second) return a.second < b.second;
            return a.millisecond < b.millisecond;
        });
    }
    
    // Encode and save with new naming convention
    if (has_snapshots) {
        std::string snapshots_dir = output_dir + "/snapshots";
        std::filesystem::create_directories(snapshots_dir);
        std::string output_file = snapshots_dir + "/snapshots_" + std::to_string(snapshots.size()) + ".bin";
        if (!encoder.encode_snapshots(snapshots, output_file, L2::ENABLE_DELTA_ENCODING)) {
            std::cerr << "Failed to encode snapshots for " << asset_code << std::endl;
            return false;
        }
    }
    
    if (has_orders) {
        std::string orders_dir = output_dir + "/orders";
        std::filesystem::create_directories(orders_dir);
        std::string output_file = orders_dir + "/orders_" + std::to_string(all_orders.size()) + ".bin";
        if (!encoder.encode_orders(all_orders, output_file, L2::ENABLE_DELTA_ENCODING)) {
            std::cerr << "Failed to encode orders for " << asset_code << std::endl;
            return false;
        }
    }
    
    if (has_snapshots || has_orders) {
        std::cout << "Successfully processed " << asset_code << " (snapshots: " << snapshots.size() 
                  << ", orders: " << all_orders.size() << ")" << std::endl;
        return true;
    }
    
    return false;
}

void encoding_worker(TaskQueue& task_queue, unsigned int core_id, std::atomic<int>& completed_tasks) {
    // Set thread affinity
    if (misc::Affinity::supported()) {
        if (!misc::Affinity::pin_to_core(core_id)) {
            std::cerr << "Warning: Failed to set thread affinity for core " << core_id << std::endl;
        }
    }
    
    EncodingTask task;
    while (task_queue.pop(task)) {
        try {
            if (process_stock_data(task.asset_dir, task.asset_code, task.date_str, task.output_base)) {
                completed_tasks.fetch_add(1);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing " << task.asset_code << ": " << e.what() << std::endl;
        }
    }
}

bool decompress_7z(const std::string& archive_path, const std::string& output_dir) {
    assert(!archive_path.empty());
    assert(!output_dir.empty());
    assert(std::filesystem::exists(archive_path));
    
    // Create output directory
    std::filesystem::create_directories(output_dir);
    
    // Use 7z command line tool for decompression
    std::string command = "7z x \"" + archive_path + "\" -o\"" + output_dir + "\" -y > /dev/null 2>&1";
    int result = std::system(command.c_str());
    
    return result == 0;
}

void decompression_worker(const std::vector<std::string>& all_archives, 
                         PingPongState& ping_pong, 
                         TaskQueue& task_queue,
                         const std::string& output_base,
                         std::atomic<int>& total_assets) {
    assert(!all_archives.empty());
    assert(!output_base.empty());
    
    // Set thread affinity to core 0
    if (misc::Affinity::supported()) {
        if (!misc::Affinity::pin_to_core(0)) {
            std::cerr << "Warning: Failed to set decompression thread affinity to core 0" << std::endl;
        }
    }
    
    for (const std::string& archive_path : all_archives) {
        std::string archive_name = std::filesystem::path(archive_path).stem().string();
        
        // Get available directory for decompression
        std::string decomp_dir = ping_pong.get_available_decomp_dir();
        
        std::cout << "Decompressing " << archive_name << ".7z to " << decomp_dir << "..." << std::endl;
        
        // Clear and prepare directory
        if (std::filesystem::exists(decomp_dir)) {
            std::filesystem::remove_all(decomp_dir);
        }
        std::filesystem::create_directories(decomp_dir);
        
        // Decompress archive
        if (!decompress_7z(archive_path, decomp_dir)) {
            std::cerr << "Failed to decompress " << archive_path << std::endl;
            continue;
        }
        
        // Find the date directory
        std::string date_dir = decomp_dir + "/" + archive_name;
        if (!std::filesystem::exists(date_dir)) {
            std::cerr << "Date directory not found after decompression: " << date_dir << std::endl;
            continue;
        }
        
        // Scan for assets and queue encoding tasks
        int assets_this_archive = 0;
        for (const auto& entry : std::filesystem::directory_iterator(date_dir)) {
            if (entry.is_directory()) {
                std::string asset_code = entry.path().filename().string();
                std::string asset_dir = entry.path().string();
                
                EncodingTask task;
                task.asset_dir = asset_dir;
                task.asset_code = asset_code;
                task.date_str = archive_name;
                task.output_base = output_base;
                
                task_queue.push(task);
                assets_this_archive++;
                total_assets.fetch_add(1);
            }
        }
        
        std::cout << "Queued " << assets_this_archive << " assets from " << archive_name 
                  << " (total queued: " << total_assets.load() << ")" << std::endl;
        
        // Signal that this directory is ready for encoding
        bool is_dir_a = (decomp_dir == ping_pong.temp_dir_a_);
        ping_pong.signal_ready(is_dir_a);
    }
    
    ping_pong.signal_decompression_finished();
    std::cout << "Decompression worker finished." << std::endl;
}

void encoding_worker_with_pingpong(TaskQueue& task_queue, 
                                  PingPongState& ping_pong,
                                  unsigned int core_id, 
                                  std::atomic<int>& completed_tasks) {
    // Set thread affinity
    if (misc::Affinity::supported()) {
        if (!misc::Affinity::pin_to_core(core_id)) {
            std::cerr << "Warning: Failed to set thread affinity for core " << core_id << std::endl;
        }
    }
    
    std::string current_working_dir;
    int tasks_in_current_dir = 0;
    
    EncodingTask task;
    while (task_queue.pop(task)) {
        try {
            // Check if we need to switch to a new working directory
            std::string task_base_dir = task.asset_dir.substr(0, task.asset_dir.find_last_of('/'));
            task_base_dir = task_base_dir.substr(0, task_base_dir.find_last_of('/'));  // Remove date dir to get temp dir
            
            if (current_working_dir.empty()) {
                // First task - wait for a ready directory
                current_working_dir = ping_pong.wait_for_ready_dir();
                if (current_working_dir.empty()) break;  // No more work
                tasks_in_current_dir = 0;
            }
            
            // Process the task if it belongs to our current working directory
            if (task_base_dir == current_working_dir) {
                if (process_stock_data(task.asset_dir, task.asset_code, task.date_str, task.output_base)) {
                    completed_tasks.fetch_add(1);
                }
                tasks_in_current_dir++;
            } else {
                // Task belongs to different directory, put it back and switch directories
                task_queue.push(task);
                
                // Finish with current directory
                if (!current_working_dir.empty()) {
                    ping_pong.finish_with_dir(current_working_dir);
                    std::cout << "Worker " << core_id << " finished processing " << tasks_in_current_dir 
                              << " tasks from " << current_working_dir << std::endl;
                }
                
                // Wait for next ready directory
                current_working_dir = ping_pong.wait_for_ready_dir();
                if (current_working_dir.empty()) break;  // No more work
                tasks_in_current_dir = 0;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing " << task.asset_code << ": " << e.what() << std::endl;
        }
    }
    
    // Finish with current directory when exiting
    if (!current_working_dir.empty()) {
        ping_pong.finish_with_dir(current_working_dir);
        std::cout << "Worker " << core_id << " finished processing " << tasks_in_current_dir 
                  << " tasks from " << current_working_dir << std::endl;
    }
}

} // namespace Parallel
} // namespace L2
