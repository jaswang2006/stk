#pragma once

namespace L2 {

    #pragma pack(push, 1)
    struct Snapshot {
    


      uint16_t time_s;            // 2 bytes - seconds in day
      int16_t latest_price_tick;  // 2 bytes - price * 100
      uint8_t trade_count;        // 1 byte
      uint32_t turnover;          // 4 bytes - RMB
      uint16_t volume;            // 2 bytes - units of 100 shares
      int16_t bid_price_ticks[5]; // 10 bytes - prices * 100
      uint16_t bid_volumes[5];    // 10 bytes - units of 100 shares
      int16_t ask_price_ticks[5]; // 10 bytes - prices * 100
      uint16_t ask_volumes[5];    // 10 bytes - units of 100 shares
      uint8_t direction;          // 1 byte
                                  // Total: 54 bytes
    };
    #pragma pack(pop)
    
    // Differential encoding configuration
    constexpr bool DIFF_FIELDS[] = {
        false, // sync
        true,  // day
        true,  // time_s
        true,  // latest_price_tick
        false, // trade_count
        false, // turnover
        false, // volume
        true,  // bid_price_ticks (array)
        false, // bid_volumes
        true,  // ask_price_ticks (array)
        false, // ask_volumes
        false  // direction
    };

}