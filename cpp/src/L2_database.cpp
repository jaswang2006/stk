#include "codec/binary_encoder_L2.hpp"
#include "codec/binary_decoder_L2.hpp"
#include <iostream>
#include <filesystem>

using namespace L2;

int main() {
    // Hardcoded paths
    const std::string input_dir = "/home/chuyin/work/stk/config/sample/L2/20240102";
    const std::string output_dir = "/home/chuyin/work/stk/output";
    
    std::cout << "L2 Database Encoder/Decoder" << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Input directory: " << input_dir << std::endl;
    std::cout << "Output directory: " << output_dir << std::endl;
    std::cout << std::endl;
    
    // Check input directory exists
    if (!std::filesystem::exists(input_dir)) {
        std::cerr << "Input directory does not exist: " << input_dir << std::endl;
        return 1;
    }
    
    // Create output directory
    std::filesystem::create_directories(output_dir);
    
    // Process encoding
    std::cout << "=== ENCODING PHASE ===" << std::endl;
    int processed_stocks = 0;
    int failed_stocks = 0;
    
    // Process each stock directory
    for (const auto& entry : std::filesystem::directory_iterator(input_dir)) {
        if (entry.is_directory()) {
            std::string stock_code = entry.path().filename().string();
            std::string stock_dir = entry.path().string();
            
            std::cout << "\nProcessing stock: " << stock_code << std::endl;
            
            if (BinaryEncoder_L2::process_stock_data(stock_dir, output_dir, stock_code)) {
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
    
    // Process decoding and print all data
    std::cout << "\n=== DECODING PHASE ===" << std::endl;
    
    // Process all binary files
    for (const auto& entry : std::filesystem::directory_iterator(output_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bin") {
            std::string filepath = entry.path().string();
            std::string filename = entry.path().filename().string();
            
            std::cout << "\nProcessing file: " << filename << std::endl;
            
            if (filename.find("_snapshots.bin") != std::string::npos) {
                // Decode snapshots
                std::vector<Snapshot> snapshots;
                if (BinaryDecoder_L2::decode_snapshots_from_binary(filepath, snapshots)) {
                    BinaryDecoder_L2::print_all_snapshots(snapshots);
                } else {
                    std::cerr << "Failed to decode snapshots from " << filepath << std::endl;
                }
            } else if (filename.find("_orders.bin") != std::string::npos) {
                // Decode orders
                std::vector<Order> orders;
                if (BinaryDecoder_L2::decode_orders_from_binary(filepath, orders)) {
                    BinaryDecoder_L2::print_all_orders(orders);
                } else {
                    std::cerr << "Failed to decode orders from " << filepath << std::endl;
                }
            }
        }
    }
    
    std::cout << "\n=== PROCESSING COMPLETE ===" << std::endl;
    return 0;
}
