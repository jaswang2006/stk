#pragma once

#include "base_compressor.hpp"
#include "rle_compressor.hpp"
#include "dictionary_compressor.hpp"
#include "bitpack_compressor.hpp"
#include <cstring>
#include <cassert>

namespace L2 {
namespace compress {

// No compression - just passes data through with minimal overhead
// Used for CUSTOM algorithm or when other algorithms are not beneficial
class NoCompressor : public BaseCompressor {
public:
    std::vector<uint8_t> compress(const void* data, size_t num_values, size_t value_size_bytes) override {
        std::vector<uint8_t> result;
        if (num_values == 0) {
            update_stats(0, 0, 0);
            return result;
        }
        
        size_t original_size = num_values * value_size_bytes;
        size_t header_size = sizeof(size_t) + sizeof(size_t);
        
        result.resize(header_size + original_size);
        size_t pos = 0;
        
        // Write header: num_values, value_size_bytes
        write_value(result, pos, num_values);
        write_value(result, pos, value_size_bytes);
        
        // Copy data directly
        std::memcpy(&result[pos], data, original_size);
        
        update_stats(original_size, result.size(), num_values);
        return result;
    }
    
    void decompress(const std::vector<uint8_t>& compressed_data, void* output, size_t num_values, size_t value_size_bytes) override {
        if (compressed_data.size() < sizeof(size_t) + sizeof(size_t)) return;
        
        size_t pos = 0;
                
        // Copy data directly
        size_t data_size = num_values * value_size_bytes;
        std::memcpy(output, &compressed_data[pos], data_size);
    }
    
    std::string_view get_algorithm_name() const override {
        return "NONE";
    }

private:
    template<typename T>
    inline void write_value(std::vector<uint8_t>& result, size_t& pos, const T& value) const {
        std::memcpy(&result[pos], &value, sizeof(T));
        pos += sizeof(T);
    }
    
    template<typename T>
    inline T read_value(const std::vector<uint8_t>& data, size_t& pos) const {
        T value;
        std::memcpy(&value, &data[pos], sizeof(T));
        pos += sizeof(T);
        return value;
    }
};

// Custom compressor that automatically selects the best compression algorithm
// based on data characteristics
class AutoSelectCompressor : public BaseCompressor {
public:
    std::vector<uint8_t> compress(const void* data, size_t num_values, size_t value_size_bytes) override {
        if (num_values == 0) {
            update_stats(0, 0, 0);
            return {};
        }
        
        // Try different algorithms and pick the best one
        std::vector<std::pair<std::unique_ptr<BaseCompressor>, std::vector<uint8_t>>> candidates;
        
        // Try RLE
        auto rle = std::make_unique<RLECompressor>();
        auto rle_result = rle->compress(data, num_values, value_size_bytes);
        candidates.emplace_back(std::move(rle), std::move(rle_result));
        
        // Try Dictionary (only for small value sizes and reasonable value counts)
        if (value_size_bytes <= 8 && num_values <= 10000) {
            auto dict = std::make_unique<DictionaryCompressor>();
            auto dict_result = dict->compress(data, num_values, value_size_bytes);
            candidates.emplace_back(std::move(dict), std::move(dict_result));
        }
        
        // Try Dynamic BitPack
        auto bitpack = std::make_unique<BitPackDynamicCompressor>();
        auto bitpack_result = bitpack->compress(data, num_values, value_size_bytes);
        candidates.emplace_back(std::move(bitpack), std::move(bitpack_result));
        
        // Try No compression
        auto none = std::make_unique<NoCompressor>();
        auto none_result = none->compress(data, num_values, value_size_bytes);
        candidates.emplace_back(std::move(none), std::move(none_result));
        
        // Find the best compression ratio
        size_t best_idx = 0;
        size_t smallest_size = candidates[0].second.size();
        
        for (size_t i = 1; i < candidates.size(); ++i) {
            if (candidates[i].second.size() < smallest_size) {
                smallest_size = candidates[i].second.size();
                best_idx = i;
            }
        }
        
        // Update stats from the best compressor
        stats_ = candidates[best_idx].first->get_stats();
        best_algorithm_name_ = std::string(candidates[best_idx].first->get_algorithm_name());
        
        // Prepend algorithm type to the result
        auto& best_result = candidates[best_idx].second;
        std::vector<uint8_t> final_result;
        final_result.reserve(1 + best_result.size());
        final_result.push_back(static_cast<uint8_t>(best_idx)); // Algorithm index
        final_result.insert(final_result.end(), best_result.begin(), best_result.end());
        
        return final_result;
    }
    
    void decompress(const std::vector<uint8_t>& compressed_data, void* output, size_t num_values, size_t value_size_bytes) override {
        if (compressed_data.empty()) return;
        
        uint8_t algo_idx = compressed_data[0];
        std::vector<uint8_t> algo_data(compressed_data.begin() + 1, compressed_data.end());
        
        std::unique_ptr<BaseCompressor> compressor;
        switch (algo_idx) {
            case 0: compressor = std::make_unique<RLECompressor>(); break;
            case 1: compressor = std::make_unique<DictionaryCompressor>(); break;
            case 2: compressor = std::make_unique<BitPackDynamicCompressor>(); break;
            case 3: compressor = std::make_unique<NoCompressor>(); break;
            default: assert(false && "Invalid algorithm index"); return;
        }
        
        compressor->decompress(algo_data, output, num_values, value_size_bytes);
    }
    
    std::string_view get_algorithm_name() const override {
        if (!best_algorithm_name_.empty()) {
            return best_algorithm_name_;
        }
        return "CUSTOM";
    }

private:
    std::string best_algorithm_name_;
};

} // namespace compress
} // namespace L2
