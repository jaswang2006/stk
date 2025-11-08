#pragma once

#include "misc/progress_parallel.hpp"

#include <mutex>
#include <string>
#include <vector>

// Forward declarations
struct SharedState;

// ============================================================================
// PHASE 1: ENCODING WORKER
// ============================================================================

void encoding_worker(SharedState &state, 
                    std::vector<size_t> &asset_id_queue, 
                    std::mutex &queue_mutex, 
                    const std::string &l2_archive_base, 
                    const std::string &temp_dir, 
                    unsigned int core_id, 
                    misc::ProgressHandle progress_handle);

