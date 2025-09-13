// L2 Database - Asset Queue + Folder Refcount Design
// Simple & robust parallel processing following exact design from workers.cpp

#include "codec/L2_DataType.hpp"
#include "codec/parallel/processing_types.hpp"
#include "codec/parallel/workers.hpp"
#include "misc/affinity.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

// pkill -f app_L2_database

// Global shutdown flag
std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal) {
  std::cout << "\nReceived signal " << signal << ", requesting graceful shutdown..." << std::endl;
  g_shutdown_requested.store(true);
  L2::Parallel::folder_queue.close();
}

// Discover all .7z archives
std::vector<std::string> discover_archives(const std::string &INPUT_DIR) {
  std::vector<std::string> archives;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(INPUT_DIR)) {
    if (entry.is_regular_file() && entry.path().extension() == ".7z") {
      archives.push_back(entry.path().string());
    }
  }
  std::sort(archives.begin(), archives.end());
  return archives;
}

int main() {
  using namespace L2::Parallel;

  // Setup signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Calculate threads
  const unsigned int num_cores = misc::Affinity::core_count();
  const unsigned int encoding_threads = num_cores - L2::DECOMPRESSION_THREADS;

  assert(L2::DECOMPRESSION_THREADS > 0 && encoding_threads > 0);

  std::cout << "L2 Processing (Asset Queue Design): "
            << L2::DECOMPRESSION_THREADS << " decomp, "
            << encoding_threads << " enc threads (max "
            << L2::MAX_TEMP_FOLDERS << " temp folders)" << std::endl;

  // Clean and setup directories
  if (std::filesystem::exists(L2::OUTPUT_DIR)) {
    std::filesystem::remove_all(L2::OUTPUT_DIR);
  }
  std::filesystem::create_directories(L2::OUTPUT_DIR);

  if (std::filesystem::exists(L2::TEMP_DIR)) {
    std::filesystem::remove_all(L2::TEMP_DIR);
  }
  std::filesystem::create_directories(L2::TEMP_DIR);

  // Discover archives and populate archive queue
  const auto archives = discover_archives(L2::INPUT_DIR);
  std::cout << "Processing " << archives.size() << " archives" << std::endl;

  {
    std::lock_guard<std::mutex> lock(archive_queue_mutex);
    for (const auto &archive : archives) {
      archive_queue.push(archive);
    }
  }

  // Initialize logging
  init_decompression_logging();

  // Launch decompression workers (producers)
  std::vector<std::thread> decompression_workers;
  for (unsigned int i = 0; i < L2::DECOMPRESSION_THREADS; ++i) {
    decompression_workers.emplace_back(decompression_worker, i);
  }

  // Launch encoding workers (consumers)
  std::vector<std::thread> encoding_workers;
  for (unsigned int i = 0; i < encoding_threads; ++i) {
    const unsigned int core_id = L2::DECOMPRESSION_THREADS + i;
    encoding_workers.emplace_back(encoding_worker, core_id);
  }

  // Wait for decompression workers to finish
  for (auto &worker : decompression_workers) {
    if (worker.joinable()) {
      worker.join();
    }
    // Check if shutdown was requested while waiting
    if (g_shutdown_requested.load()) {
      // Detach remaining workers and exit
      for (auto &remaining_worker : decompression_workers) {
        if (remaining_worker.joinable()) {
          remaining_worker.detach();
        }
      }
      std::exit(0);
    }
  }

  // After all decompression workers finish, close the folder queue
  folder_queue.close();

  close_decompression_logging();
  std::cout << "Decompression phase complete" << std::endl;

  // Wait for encoding workers to finish (they should be fast)
  for (auto &worker : encoding_workers) {
    if (worker.joinable()) {
      worker.join();
    }
    // Check if shutdown was requested while waiting
    if (g_shutdown_requested.load()) {
      // Let remaining workers finish since encoding is fast (~1ms)
      break;
    }
  }

  std::cout << "Processing complete - all assets processed" << std::endl;

  return 0;
}