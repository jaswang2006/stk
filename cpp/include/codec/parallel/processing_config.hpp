#pragma once

#include <cstdint>

namespace L2 {
namespace Parallel {

/**
 * Configuration parameters for parallel L2 data processing pipeline
 * Organized parameters for decompression threads, buffers, and encoding workers
 */
struct ProcessingConfig {
    // Decompression configuration
    uint32_t decompression_threads = 1;      // Number of parallel decompression threads
    uint32_t decompression_buffers = 2;      // Number of temporary buffer directories for decompression
    
    // Encoding configuration  
    uint32_t encoding_threads = 0;           // Number of encoding threads (0 = auto-detect: cores - decompression_threads)
    
    // Core assignment strategy
    bool use_core_affinity = true;           // Whether to pin threads to specific cores
    uint32_t decompression_core_start = 0;   // Starting core for decompression threads
    uint32_t encoding_core_start = 0;        // Starting core for encoding threads (0 = auto: after decompression cores)
    
    // Error handling
    bool terminate_on_error = true;          // Immediately terminate program on any error
    
    // Paths
    const char* input_base = "/mnt/dev/sde/A_stock/L2";
    const char* output_base = "/mnt/dev/sde/A_stock/L2_binary";
    const char* temp_base = "/tmp/L2_processing";
    
    /**
     * Auto-configure based on available CPU cores
     * @param total_cores Total number of CPU cores available
     */
    void auto_configure(uint32_t total_cores) {
        // Reserve cores for decompression, rest for encoding
        if (decompression_threads > total_cores) {
            decompression_threads = total_cores / 2; // At least half for decompression
            if (decompression_threads == 0) decompression_threads = 1;
        }
        
        // Auto-configure encoding threads if not set
        if (encoding_threads == 0) {
            encoding_threads = total_cores - decompression_threads;
            if (encoding_threads == 0) encoding_threads = 1;
        }
        
        // Auto-configure core assignments
        if (encoding_core_start == 0) {
            encoding_core_start = decompression_threads;
        }
        
        // Scale buffers with decompression threads (2x for overlap)
        if (decompression_buffers < decompression_threads * 2) {
            decompression_buffers = decompression_threads * 2;
        }
    }
    
    /**
     * Validate configuration parameters
     */
    bool validate() const {
        return decompression_threads > 0 && 
               decompression_buffers >= decompression_threads &&
               encoding_threads > 0;
    }
};

// Global configuration instance
extern ProcessingConfig g_config;

} // namespace Parallel
} // namespace L2
