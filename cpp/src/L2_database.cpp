#include "codec/binary_encoder_L2.hpp"
#include "codec/binary_decoder_L2.hpp"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <map>

using namespace L2;

// Function to compare encoded data and decoded data for bit-exact match
bool compare_snapshots_bit_exact(const std::vector<Snapshot>& original, const std::vector<Snapshot>& decoded) {
    if (original.size() != decoded.size()) {
        std::cerr << "Size mismatch: original=" << original.size() << ", decoded=" << decoded.size() << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < original.size(); ++i) {
        if (std::memcmp(&original[i], &decoded[i], sizeof(Snapshot)) != 0) {
            std::cerr << "Bit mismatch at index " << i << std::endl;
            
            // Print detailed comparison for debugging
            const Snapshot& orig = original[i];
            const Snapshot& dec = decoded[i];
            
            std::cout << "Original[" << i << "]: ";
            std::cout << "hour=" << (int)orig.hour << ", minute=" << (int)orig.minute << ", second=" << (int)orig.second << std::endl;
            std::cout << "Decoded [" << i << "]: ";
            std::cout << "hour=" << (int)dec.hour << ", minute=" << (int)dec.minute << ", second=" << (int)dec.second << std::endl;
            
            return false;
        }
    }
    
    std::cout << "✓ Snapshots bit-exact comparison PASSED for " << original.size() << " items" << std::endl;
    return true;
}

bool compare_orders_bit_exact(const std::vector<Order>& original, const std::vector<Order>& decoded) {
    if (original.size() != decoded.size()) {
        std::cerr << "Size mismatch: original=" << original.size() << ", decoded=" << decoded.size() << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < original.size(); ++i) {
        if (std::memcmp(&original[i], &decoded[i], sizeof(Order)) != 0) {
            std::cerr << "Bit mismatch at index " << i << std::endl;
            
            // Print detailed comparison for debugging
            const Order& orig = original[i];
            const Order& dec = decoded[i];
            
            std::cout << "Original[" << i << "]: ";
            std::cout << "hour=" << (int)orig.hour << ", minute=" << (int)orig.minute << ", second=" << (int)orig.second 
                      << ", price=" << orig.price << std::endl;
            std::cout << "Decoded [" << i << "]: ";
            std::cout << "hour=" << (int)dec.hour << ", minute=" << (int)dec.minute << ", second=" << (int)dec.second 
                      << ", price=" << dec.price << std::endl;
            
            return false;
        }
    }
    
    std::cout << "✓ Orders bit-exact comparison PASSED for " << original.size() << " items" << std::endl;
    return true;
}

int main() {
    // Hardcoded paths
    const std::string input_dir = "/home/chuyin/work/stk/config/sample/L2/20240102";
    const std::string output_dir = "/home/chuyin/work/stk/output";
    
    std::cout << "L2 Database Encoder/Decoder" << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Input directory: " << input_dir << std::endl;
    std::cout << "Output directory: " << output_dir << std::endl;
    std::cout << std::endl;
    
    // Create reusable encoder and decoder instances with optimized capacity
    // Estimated sizes based on typical stock data files
    BinaryEncoder_L2 encoder(200000, 1000000);  // 200k snapshots, 1M orders per stock
    BinaryDecoder_L2 decoder(200000, 1000000);  // Same capacities for decoder
    
    // Check input directory exists
    if (!std::filesystem::exists(input_dir)) {
        std::cerr << "Input directory does not exist: " << input_dir << std::endl;
        return 1;
    }
    
    // Create output directory
    std::filesystem::create_directories(output_dir);
    
    // Process encoding and store original data for validation
    std::cout << "=== ENCODING PHASE ===" << std::endl;
    std::cout << "Delta encoding is " << (L2::ENABLE_DELTA_ENCODING ? "ENABLED" : "DISABLED") << std::endl;
    int processed_stocks = 0;
    int failed_stocks = 0;
    std::map<std::string, std::vector<Snapshot>> original_snapshots;
    std::map<std::string, std::vector<Order>> original_orders;
    
    // Process each stock directory
    for (const auto& entry : std::filesystem::directory_iterator(input_dir)) {
        if (entry.is_directory()) {
            std::string stock_code = entry.path().filename().string();
            std::string stock_dir = entry.path().string();
            
            std::cout << "\nProcessing stock: " << stock_code << std::endl;
            
            // Use process_stock_data to encode and get original data for validation
            std::vector<Snapshot> snapshots;
            std::vector<Order> orders;
            
            if (encoder.process_stock_data(stock_dir, output_dir, stock_code, &snapshots, &orders)) {
                // Store original data for validation
                if (!snapshots.empty()) {
                    original_snapshots[stock_code] = std::move(snapshots);
                }
                if (!orders.empty()) {
                    original_orders[stock_code] = std::move(orders);
                }
                
                processed_stocks++;
                std::cout << "Successfully processed " << stock_code << std::endl;
            } else {
                failed_stocks++;
                std::cerr << "Failed to process " << stock_code << std::endl;
            }
        }
    }
    
    std::cout << "\n=== ENCODING SUMMARY ===" << std::endl;
    std::cout << "Processed stocks: " << processed_stocks << std::endl;
    std::cout << "Failed stocks: " << failed_stocks << std::endl;
    
    if (failed_stocks > 0) {
        std::cerr << "Encoding failed for some stocks, stopping." << std::endl;
        return 1;
    }
    
    // Process decoding and validation
    std::cout << "\n=== DECODING & VALIDATION PHASE ===" << std::endl;
    
    // Process all binary files and validate against original data
    for (const auto& entry : std::filesystem::directory_iterator(output_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bin") {
            std::string filepath = entry.path().string();
            std::string filename = entry.path().filename().string();
            
            std::cout << "\nProcessing file: " << filename << std::endl;
            
            if (filename.find("_snapshots") != std::string::npos) {
                // Extract stock code from filename
                std::string stock_code = filename.substr(0, filename.find("_snapshots"));
                
                // Decode snapshots
                std::vector<Snapshot> decoded_snapshots;
                if (decoder.decode_snapshots(filepath, decoded_snapshots)) {
                    // BinaryDecoder_L2::print_all_snapshots(decoded_snapshots);
                    
                    // Validate against original data
                    if (original_snapshots.find(stock_code) != original_snapshots.end()) {
                        if (compare_snapshots_bit_exact(original_snapshots[stock_code], decoded_snapshots)) {
                            std::cout << "✓ " << stock_code << " snapshots validation PASSED" << std::endl;
                        } else {
                            std::cerr << "✗ " << stock_code << " snapshots validation FAILED" << std::endl;
                        }
                    }
                } else {
                    std::cerr << "Failed to decode snapshots from " << filepath << std::endl;
                }
            } else if (filename.find("_orders") != std::string::npos) {
                // Extract stock code from filename
                std::string stock_code = filename.substr(0, filename.find("_orders"));
                
                // Decode orders
                std::vector<Order> decoded_orders;
                if (decoder.decode_orders(filepath, decoded_orders)) {
                    // BinaryDecoder_L2::print_all_orders(decoded_orders);
                    
                    // Validate against original data
                    if (original_orders.find(stock_code) != original_orders.end()) {
                        if (compare_orders_bit_exact(original_orders[stock_code], decoded_orders)) {
                            std::cout << "✓ " << stock_code << " orders validation PASSED" << std::endl;
                        } else {
                            std::cerr << "✗ " << stock_code << " orders validation FAILED" << std::endl;
                        }
                    }
                } else {
                    std::cerr << "Failed to decode orders from " << filepath << std::endl;
                }
            }
        }
    }
    
    std::cout << "\n=== PROCESSING COMPLETE ===" << std::endl;
    return 0;
}
