#ifndef AMD_AMA_HKEX_STRUCT_H_
#define AMD_AMA_HKEX_STRUCT_H_

#include "../ama_datatype.h"

#pragma pack(push)
#pragma pack(1)

namespace amd { namespace ama {

/*
精度说明:
本文中所有字段带有类型说明的相比如实际值有扩大倍数,详细倍数扩大规则可参考ama_struct.h
*/

/**
 * @name MDHKExSnapshot 商业港股股票快照数据结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExSnapshot
{
    int32_t market_type;                                                    // 市场类型
    char    security_code[ConstField::kSecurityCodeLen];                    // 证券代码
    int64_t orig_time;                                                      // 行情时间(为YYYYMMDDHHMMSSsss)
    int64_t pre_close_price;                                                // 昨收价(类型:Price6)
    int64_t last_price;                                                     // 最新价(类型:Price6)
    int64_t open_price;                                                     // 开盘价(类型:Price6)
    int64_t high_price;                                                     // 最高价(类型:Price6)
    int64_t low_price;                                                      // 最低价(类型:Price6)
    int64_t close_price;                                                    // 收市价(类型:Price6)
    int64_t total_volume_trade;                                             // 成交量(类型:Qty2)
    int64_t total_value_trade;                                              // 成交额(类型:Amt5)
    int64_t short_volume_shares;                                            // 沽空量(类型:Qty2)
    int64_t short_value_turnover;                                           // 沽空额(类型:Amt5)
    uint8_t variety_category;                                               // 品种类别
};

/**
 * @name MDHKExCallAuctionSnapshot 商业港股集合竞价快照数据结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExCallAuctionSnapshot
{
    int32_t market_type;                                                    // 市场类型
    char    security_code[ConstField::kSecurityCodeLen];                    // 证券代码
    int64_t orig_time;                                                      // 行情时间(为YYYYMMDDHHMMSSsss)
    int64_t reff_price;                                                     // CAS、POS阶段参考价(类型:Price6)
    int64_t lower_price;                                                    // 下限价(类型:Price6)
    int64_t upper_price;                                                    // 上限价(类型:Price6)
    char    ord_imb_direction;                                              // CAS、POS阶段未能配对买卖盘的方向('N':买卖盘量相等,'B':买盘比卖盘多,'S':卖盘比买盘多,' ':不适用(未有参考平衡价格))
    int64_t ord_imb_volume;                                                 // CAS、POS 阶段未能配对买卖盘的数量(类型:Qty2)
    int64_t virtual_price;                                                  // 虚拟开盘参考价/虚拟收盘参考价（根据所处集合竞价阶段）(类型:Price6)
    int64_t virtual_auction_volume;                                         // 虚拟匹配量（根据所处集合竞价阶段）(类型:Qty2)
    uint8_t variety_category;                                               // 品种类别
};

/**
 * @name MDHKExConnectTurnoverSnapshot 商业港股港股通成交额快照数据结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExConnectTurnoverSnapshot
{
    int32_t market_type;                                                    // 市场类型
    char    market[ConstField::kMarketLen];                                 // 市场: SH--上海交易所，SZ-- 深圳交易所
    int64_t orig_time;                                                      // 时间（为YYYYMMDDHHMMSSsss）
    char    direction[ConstField::kDirLen];                                 // 交易方向: 'NB' -- 北向交易, 'SB'-- 南向交易
    int64_t buy_turnover;                                                   // 买入额(类型:Amt5)
    int64_t sell_turnover;                                                  // 卖出额(类型:Amt5)
    int64_t buy_sell_turnover;                                              // 买卖总额(类型:Amt5)
    uint8_t variety_category;                                               // 品种类别
};

/**
 * @name MDHKExListItem 买卖单序列结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExListItem
{
    int64_t order_price;                  // 挂单价格(类型:Price6)
    int64_t order_volume;                 // 挂单数量(类型:Qty2)
    int64_t num_of_orders;                // 挂单经纪席位数目
};


/**
 * @name MDHKExOrderSnapshot 商业港股委托挂单数据结构(嵌套在商业港股股票快照结构中,FiuType=FIU_ORDER,消息体=PBOrder)
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExOrderSnapshot
{
    int32_t market_type;                                                    // 市场类型
    char    security_code[ConstField::kSecurityCodeLen];                    // 证券代码
    int64_t orig_time;                                                      // 时间(为YYYYMMDDHHMMSSsss)
    MDHKExListItem bid_list[ConstField::kHKExPositionLevelLen];             // 买单序列
    MDHKExListItem ask_list[ConstField::kHKExPositionLevelLen];             // 卖单序列
    uint8_t variety_category;                                               // 品种类别
};

/**
 * @name MDHKExDetailItem 明细结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExDetailItem
{
    uint8_t level;                                                      // 档位信息
    uint16_t broker_num;                                                // 席位号
};

/**
 * @name MDHKExOrderBrokerSnapshot 商业港股经纪席位数据结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExOrderBrokerSnapshot
{
    int32_t market_type;                                                    // 市场类型
    char    security_code[ConstField::kSecurityCodeLen];                    // 证券代码
    int64_t orig_time;                                                      // 时间(为YYYYMMDDHHMMSSsss)
    uint8_t side;                                                           // 买卖方向('B'买,'S'卖)
    char    broker_flag;                                                    // 是否还有未展示完的席位排位(Y-是 N-否)
    MDHKExDetailItem  detail[ConstField::kDetailLen];                       // 买卖经纪席位明细(所有档位的席位号数量之和最多有40个)
    uint8_t variety_category;                                               // 品种类别
};

/**
 * @name MDHKExTickExecution 商业港股逐笔成交数据结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExTickExecution
{
    int32_t market_type;                                                    // 市场类型
    char    security_code[ConstField::kSecurityCodeLen];                    // 证券代码
    int64_t orig_time;                                                      // 时间(为YYYYMMDDHHMMSSsss)
    int64_t appl_seq_num;                                                   // 序号
    uint8_t side;                                                           // 买卖方向('B'主动买入,'-'中性盘,'S'主动卖出)
    int64_t exec_price;                                                     // 委托价格(类型:Price6)
    int64_t exec_volume;                                                    // 委托数量(类型:Qty2)
    uint8_t exec_type;                                                      // 成交类型
    //************************************成交类型取值及定义******************************************************
    //0-自动对盘 4-开市前成交盘 22-非自动对盘 100-同一证券商自动对盘
    //101-同一证券商非自动对盘 102-碎股交易 103-竞价交易
    char cancel_flag;                                                       // 撤单标识(Y 是/N 否)
    uint8_t variety_category;                                               // 品种类别
};

/**
 * @name MDHKExIndexSnapshot 商业港股指数行情快照数据结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExIndexSnapshot
{
    int32_t market_type;                                                    // 市场类型
    char    security_code[ConstField::kSecurityCodeLen];                    // 证券代码(00000 为特殊代码揭示指数数量)
    int64_t orig_time;                                                      // 时间(为YYYYMMDDHHMMSSsss)
    char    index_status;                                                   // 指数状态
    int64_t pre_close_price;                                                // 昨收价(类型:Price6)
    int64_t open_price;                                                     // 开盘价(类型:Price6)
    int64_t high_price;                                                     // 最高价(类型:Price6)
    int64_t low_price;                                                      // 最低价(类型:Price6)
    int64_t close_price;                                                    // 收市价(类型:Price6)
    int64_t last_price;                                                     // 最新价(类型:Price6)(当指数代码为"00000"时,揭示当日发送的指数个数,无扩大倍数)
    int64_t total_volume_trade;                                             // 成交量(类型:Qty2)
    int64_t total_value_trade;                                              // 成交额(类型:Amt5)
    int64_t change;                                                         // 涨跌(类型:Rate6)
    int64_t ratio_of_change;                                                // 涨跌%(类型:Rate6)
    int64_t eas_value;                                                      // 预估结算值(类型:Amt5)
    uint8_t variety_category;                                               // 品种类别
    int32_t index_time;                                                     // 指数发布时间(为HHMMSS)
    char    index_source;                                                   // 指数来源
    char    currency_code[ConstField::kCurrencyCodeLen];                    // 汇率代码
};

/**
 * @name MDHKExOddLotOrderItem 碎股订单序列结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExOddLotOrderItem
{
    int64_t order_id;                     // 订单编号
    int64_t price;                        // 价格(类型:Price6)
    int64_t order_volume;                 // 量(类型:Qty2)
    int32_t broker_id;                    // 经纪人编号
    uint8_t side;                         // 买卖方向 (0:Bid,1:Offer)
};


/**
 * @name MDHKExStockOddLot 商业港股碎股订单结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExStockOddLot
{
    int32_t market_type;                                                    // 市场类型
    char    security_code[ConstField::kSecurityCodeLen];                    // 证券代码
    int64_t orig_time;                                                      // 时间(为YYYYMMDDHHMMSSsss)
    MDHKExOddLotOrderItem order_list[ConstField::kOddLotOderLen];           // 碎股订单序列（前20条）
    uint8_t variety_category;                                               // 品种类别
};


/**
 * @name MDHKExMergeSnapshot 商业港股股票快照和委托挂单数据结构
 * 字段类型的扩大倍数参考 华锐高速行情平台AMD转码API开发指南中的 "精度说明"
 * @{ 
 */
struct MDHKExMergeSnapshot
{
    int32_t market_type;                                                    // 市场类型
    char    security_code[ConstField::kSecurityCodeLen];                    // 证券代码
    int64_t orig_time;                                                      // 行情时间(为YYYYMMDDHHMMSSsss)
    int64_t pre_close_price;                                                // 昨收价(类型:Price6)
    int64_t last_price;                                                     // 最新价(类型:Price6)
    int64_t open_price;                                                     // 开盘价(类型:Price6)
    int64_t high_price;                                                     // 最高价(类型:Price6)
    int64_t low_price;                                                      // 最低价(类型:Price6)
    int64_t close_price;                                                    // 收市价(类型:Price6)
    int64_t total_volume_trade;                                             // 成交量(类型:Qty2)
    int64_t total_value_trade;                                              // 成交额(类型:Amt5)
    int64_t short_volume_shares;                                            // 沽空量(类型:Qty2)
    int64_t short_value_turnover;                                           // 沽空额(类型:Amt5)
    uint8_t variety_category;                                               // 品种类别
    int64_t norminal_price;                                                 // 按盘价(类型:Price6)
    int64_t yield;                                                          // 债券收益(类型:Price6)
    int32_t trading_status;                                                 // 交易状态
    MDHKExListItem bid_list[ConstField::kHKExPositionLevelLen];             // 买单序列
    MDHKExListItem ask_list[ConstField::kHKExPositionLevelLen];             // 卖单序列
};

}; // end of ama
}; // end of amd

#pragma pack(pop)
#endif