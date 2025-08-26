#pragma once

#include <cstdint>

//深圳L2行情数据结构
namespace L2_sz {

    //集中竞价交易行情快照消息(300111)
    struct sz_stk_snapshot {
        // Standard Header
        uint32_t StandardHeader;              // 消息头 MsgType=300111
        
        // Basic Info
        int64_t OrigTime;                     // 数据生成时间 (YYYYMMDDHHMMSSsss)
        uint16_t ChannelNo;                   // 频道代码
        char MDStreamID[3];                   // 行情类别 现货:010 期权:040
        char SecurityID[8];                   // 证券代码
        char SecurityIDSource[4];             // 证券代码源 深交:102 港交:103
        char TradingPhaseCode[8];             // 产品所处的交易阶段代码
                                              // [0]: S:启动 O:开盘 T:连续竞价 B:休市 C:收盘集合竞价 E:已闭市 H:零时停牌 A:盘后交易 V:波动性中断
                                              // [1]: 0:正常 1:全天停牌
        
        // Price and Volume Info
        int64_t PrevClosePx;                  // 昨收价 Int64-N13(4)
        int64_t NumTrades;                    // 成交笔数
        int64_t TotalVolumeTrade;             // 成交总量 Int64-N15(2)
        int64_t TotalValueTrade;              // 成交总金额 Int64-N18(4)
        
        // MD Entries
        uint32_t NoMDEntries;                 // 行情条目个数
        
        struct MDEntry {
            char MDEntryType[2];              // MD Entry Type:
                                              // 0=Buy (买入)
                                              // 1=Sell (卖出)
                                              // 2=Last Price (最近价)
                                              // 4=Open Price (开盘价)
                                              // 7=High Price (最高价)
                                              // 8=Low Price (最低价)
                                              // x1=Price Change 1 (升跌一)
                                              // x2=Price Change 2 (升跌二)
                                              // x3=Buy Summary (买入汇总-总量及加权平均价)
                                              // x4=Sell Summary (卖出汇总-总量及加权平均价)
                                              // x5=Stock PE Ratio 1 (股票市盈率一)
                                              // (not used) x6=Stock PE Ratio 2 (股票市盈率二)
                                              // x7=Fund T-1 NAV (基金T-1日净值)
                                              // x8=Fund Real-time NAV/IOPV (基金实时参考净值/ETF的IOPV)
                                              // x9=Warrant Premium Rate (权证溢价率)
                                              // xe=Upper Limit Price (涨停价)
                                              // xf=Lower Limit Price (跌停价)
                                              // (not used) xg=Contract Open Interest (合约持仓量)
                                              // (not used) xi=Reference Price (参考价)
            int64_t MDEntryPx;                // 价格 Int64-N18(6)
            int64_t MDEntrySize;              // 数量 Int64-N15(2)
            uint16_t MDPriceLevel;            // 买卖盘档位
            int64_t NumberOfOrders;           // 价位总委托笔数
            uint32_t NoOrders;                // 价位揭示委托笔数
            int64_t OrderQty;                 // 委托数量 Int64-N15(2)
        };
        
        // MDEntry md_entries[];              // Flexible array member
    };

    //集中竞价交易逐笔成交消息(300191)
    struct sz_stk_trade{
        // Standard Header
        uint32_t StandardHeader;              // 消息头 MsgType=300191

        // Basic Info
        uint16_t ChannelNo;                   // 频道代码
        int64_t ApplSeqNum;                   // 消息记录号 (从1开始)
        char MDStreamID[3];                   // 行情类别 现货:011 期权:041
        char SecurityID[8];                   // 证券代码
        char SecurityIDSource[4];             // 证券代码源 深交:102 港交:103
        
        int64_t Price;                        // 委托价格 Int64-N13(4)
        int64_t OrderQty;                     // 委托数量 Int64-N15(2)
        char Side;                            // 买卖方向 1:买 2:卖 G:借入 F:出借
        int64_t TransactTime;                 // 委托时间 (YYYYMMDDHHMMSSsss)
    };

    //集中竞价交易逐笔委托消息(300192)
    struct sz_stk_order{
        // Standard Header
        uint32_t StandardHeader;              // 消息头 MsgType=300192

        // Basic Info
        uint16_t ChannelNo;                   // 频道代码
        int64_t ApplSeqNum;                   // 消息记录号 (从1开始)
        char MDStreamID[3];                   // 行情类别 现货:010 期权:040
        char SecurityID[8];                   // 证券代码
        char SecurityIDSource[4];             // 证券代码源 深交:102 港交:103

        int64_t Price;                        // 委托价格 Int64-N13(4)
        int64_t OrderQty;                     // 委托数量 Int64-N15(2)
        char Side;                            // 买卖方向 1:买 2:卖 G:借入 F:出借
        char OrderType;                       // 订单类型 1:市价 2:限价 U:本方最优(防止高频套利)
        int64_t TransactTime;                 // 委托时间 (YYYYMMDDHHMMSSsss)
    };

} // namespace L2_sz


namespace L2_sh {

} // namespace L2_sh
