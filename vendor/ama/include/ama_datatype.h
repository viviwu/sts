/**
 * @file         ama_datatype.h
 * @author       郭光葵
 * @mail         guoguangkui@archforce.com.cn
 * @created time Thu 21 Sep 2017 09:52:50 AM CST
 *
 * Copyright (c) 2018 Archforce Financial Technology.  All rights reserved.
 * Redistribution and use in source and binary forms, with or without  modification, are not permitted.
 * For more information about Archforce, welcome to archforce.cn.
 */

#ifndef AMD_AMA_DATATYPE_H_
#define AMD_AMA_DATATYPE_H_

#include <stdint.h>
namespace amd { namespace ama {

#define ID_BT_INDEX                   "01000"   //指数
#define ID_BT_SHARES_A                "02001"   //主板A股	                  
#define ID_BT_SHARES_B                "02002"   //主板B股	                  
#define ID_BT_SHARES_G                "02003"   //创业板股	                  
#define ID_BT_SHARES_KCB              "02004"   //科创板股	                  
#define ID_BT_SHARES_LIST             "02005"   //股转系统挂牌股	            
#define ID_BT_SHARES_PRE              "02006"   //优先股	                    
#define ID_BT_SHARES_PZ_ZB            "02007"   //主板存托凭证	              
#define ID_BT_SHARES_PZ_CYB           "02008"   //创业板存托凭证	            
#define ID_BT_SHARES_PZ_KCB           "02009"   //科创板存托凭证	            
#define ID_BT_SHARES_OTE              "02999"   //其他股票	                  
#define ID_BT_FUND_ET                 "03001"   //交易型开放式指数基金（ETF） 
#define ID_BT_FUND_LO                 "03002"   //上市型开放式基金（LOF）	    
#define ID_BT_FUND_GR                 "03003"   //分级子基金	                
#define ID_BT_FUND_OPE                "03004"   //未上市开放基金（仅申赎）	  
#define ID_BT_FUND_CLOS               "03005"   //封闭式基金	                
#define ID_BT_FUND_REIT               "03006"   //基础设施基金（REITs）	      
#define ID_BT_FUND_OT                 "03999"   //其他基金	                  
#define ID_BT_BOND_CO                 "04001"   //可转债	                    
#define ID_BT_BOND_N                  "04002"   //国债（含地方债）	          
#define ID_BT_BOND_COM                "04003"   //公司债	                    
#define ID_BT_BOND_EN                 "04004"   //企业债	                    
#define ID_BT_BOND_P                  "04005"   //私募债	                    
#define ID_BT_BOND_PP_E	              "04006"   //可交换私募债	             	
#define ID_BT_BOND_COM_S              "04007"   //证券公司次级债	           	
#define ID_BT_BOND_S                  "04008"   //证券公司短期债	            
#define ID_BT_BOND_OT                 "04999"   //其他债券	                  
#define ID_BT_REPO_ZY                 "05001"   //质押式回购	                
#define ID_BT_REPO_BOND_MD            "05002"   //买断式债券回购	            
#define ID_BT_REPO_B                  "05003"   //报价回购	                  
#define ID_BT_OPTION_C_G              "06001"   //个股认购期权	              
#define ID_BT_OPTION_P_G              "06002"   //个股认沽期权	              
#define ID_BT_OPTION_C_ID             "06003"   //指数认购期权	              
#define ID_BT_OPTION_P_IDX            "06004"   //指数认沽期权
#define ID_BT_OPTION_C_CMD            "06005"   //商品认购期权
#define ID_BT_OPTION_P_CM             "06006"   //商品认沽期权
#define ID_BT_FUTURE                  "06007"   //期货
#define ID_BT_FUTURE_I                "06008"   //指数期货(规划中)
#define ID_BT_FUTURE_SY               "06009"   //组合
#define ID_BT_FUTURE_SP               "06010"   //即期
#define ID_BT_EFPs                    "06011"   //期转现
#define ID_BT_TAS                     "06012"   //TAS
#define ID_BT_Q                       "07001"   //权证	                      
#define ID_BT_X                       "07002"   //信托	                      
#define ID_BT_BB_G                    "07003"   //港股股本	                  
#define ID_BT_PLAN_CA                 "99001"   //集合资产管理计划	          
#define ID_BT_AB                      "99002"   //资产支持证券	              
#define ID_BT_C                       "99003"   //控制指令	   


/*
 * @brief property 配置项枚举
 */
namespace property
{
    /*
        设置各数据通道关键线程绑核参数，格式为CPU核的列表,比如"3,4"，表示使用第三与第四个 CPU，避免与非关键线程资源抢占影响性能.
        case 1: 设置的CPU核列表中数量小于实际关键线程数, 只会绑定部分关键线程，同时会有告警.
        case 2: 如果设置的CPU核列表中数量大于实际关键线程数,会随机绑定列表中的cpu.
        case 3: 建议绑核列表中的cpu先进行孤立,避免其他程序抢占cpu资源。
                (linux孤立CPU步骤:  1. 找到系统grub文件,一般路径为/etc/default/grub,具体路径以实际为准 
                                    2. sudo vi /etc/default/grub, 以孤立cpu 3,4为例, GRUB_CMDLINE_LINUX="isolcpus=3,4 nohz_full=3,4 rcu_nocbs=3,4"
                                    3. 生成新的grub引导文件: sudo grub2-mkconfig -o /boot/grub2/grub.cfg
                                    4. 重启系统,生效孤立配置)
        note: 绑核暂时只对linux 系统生效,windows 系统暂时不支持绑核操作
    */
    static const char* const kTCPThreadAffinity = "TCPThreadAffinity";              // 将TCP数据通道内关键处理线程绑定到CPU核(关键线程数量为2)
    static const char* const kUDPThreadAffinity = "UDPThreadAffinity";              // 将UDP数据通道内关键处理线程绑定到CPU核(关键线程数量为2)
    static const char* const kAMIThreadAffinity = "AMIThreadAffinity";              // 将AMI数据通道内关键处理线程绑定到CPU核(关键线程数量为3)
    static const char* const kFPGAThreadAffinity = "FPGAThreadAffinity";            // 将FPGA数据通道内关键处理线程绑定到CPU核(关键线程数量视实际情况而定)
                                                                                    // note: 正常启动原地递交模式单路FPGA关键线程数为1,其他情况按实际情况设置
    static const char* const kMDDPThreadAffinity = "MDDPThreadAffinity";            // 将MDDP数据通道内关键处理线程绑定到CPU核(单组播端口关键线程数量为1,其他情况按实际情况设置)
    static const char* const kRDMAThreadAffinity = "RDMAThreadAffinity";            // 将RDMA数据通道内关键处理线程绑定到CPU核(sse市场关键线程数量为3,szse市场关键线程数量为1)
    static const char* const kEXAThreadAffinity = "EXAThreadAffinity";              // 将EXA数据通道内关键处理线程绑定到CPU核(sse市场关键线程数量为3,szse市场关键线程数量为1)
    static const char* const kPCAPThreadAffinity = "PCAPThreadAffinity";            // 将PCAP数据通道内关键处理线程绑定到CPU核(sse市场关键线程数量为3,szse市场关键线程数量为1)
    static const char* const kOrderBookThreadAffinity = "OrderBookThreadAffinity";  // 将委托簿本地构建关键处理线程绑定到CPU核(关键线程数为设置的委托簿线程数量*2*市场数量)

    static const char* const kEnableSseLocalOrderBook = "EnableSseLocalOrderBook";  // 是否打开上海委托簿，默认打开(仅本地构建委托簿生效)
    static const char* const kEnableSzseLocalOrderBook = "EnableSzseLocalOrderBook";// 是否打开深圳委托簿，默认打开(仅本地构建委托簿生效)
    static const char* const kEnableTCPPolling = "EnableTCPPolling";                // 是否打开TCP模式polling，默认打开(仅linux版本生效)
    static const char* const kEnableUDPPolling = "EnableUDPPolling";                // 是否打开UDP模式polling，默认打开
    static const char* const kKeepOrderTimeoutMS = "KeepOrderTimeoutMS";            // 保序超时时间，单位毫秒，默认为10ms

    static const char* const kAMIMessageIP = "AMIMessageIP";                        // AMI通道使用的本地IP地址(若开启),默认为空
    static const char* const kUDPMessageIP = "UDPMessageIP";                        // UDP通道使用的本地IP地址(若开启),默认为空
    static const char* const kFPGAMessageIP = "FPGAMessageIP";                      // FPGA通道使用的本地IP地址(若开启),默认为空
    static const char* const kTCPMessageIP = "TCPMessageIP";                        // TCP通道使用的本地IP地址(若开启),默认为空,若配置为S卡IP则支持efvi加速(仅linux版本生效)

    /**
     * @brief 是否启用MDHKExMergeSnapshot(港股股票快照和委托挂单数据结构),默认关闭。
     *        启用此开关并且订阅包含MDHKExSnapshot、MDHKExOrderSnapshot才能收到MDHKExMergeSnapshot数据。
     *        启用后Spi将收不到以下回调:OnMDHKExSnapshot(港股股票快照数据结构)、OnMDHKExOrderSnapshot(港股委托挂单数据结构);能收到OnMDHKExMergeSnapshot回调
     */
    static const char* const kEnableHKExMergeSnapshot = "EnableHKExMergeSnapshot";
};

/**
 * @brief 市场类型定义
 */
class MarketType
{
public:
    enum 
    { 
        kNone       = 0,                ///< 表示全市场 
        kNEEQ       = 2,                ///< 北交所
        kSHFE       = 3,                ///< 上期所
        kCFFEX      = 4,                ///< 中金所
        kDCE        = 5,                ///< 大商所
        kCZCE       = 6,                ///< 郑商所
        kINE        = 7,                ///< 上期能源
        kNASDAQ     = 12,               ///< 纳交所
        kGFEX       = 16,               ///< 广期所
        kSSE        = 101,              ///< 上交所
        kSZSE       = 102,              ///< 深交所
        kHKEx       = 103,              /* < 港交所(港股通行情数据通过深交所和上交所的港股通获取, 市场类型为kSZSE/kSSE,
                                                商业港股行情对应的市场类型为kHKEx) */
        kCFETS      = 104,              ///< 银行间市场
        kMax        = 150               ///< 市场类型最大值
    };
};

/**
 * @brief 委托簿构建类型定义
 */
class OrderBookType {
public:
    static const uint8_t kNone                                 = 0;   // 不开启委托簿
    static const uint8_t kLocalOrderBook                       = 1;   // 本地构建委托簿,向服务端请求同步初始状态(需要订阅相应的逐笔成交和逐笔委托)
    static const uint8_t kServerOrderBook                      = 2;   // 服务端远程TCP方式推送委托簿数据
};

/**
 * @brief 品种类型定义
 */
class VarietyCategory {
public:
    static const uint8_t kNone                                 = 0;   // None
    static const uint8_t kStock                                = 1;   // 股票
    static const uint8_t kFund                                 = 2;   // 基金
    static const uint8_t kBond                                 = 3;   // 债券
    static const uint8_t kOption                               = 4;   // 期权
    static const uint8_t kIndex                                = 5;   // 指数
    static const uint8_t kHKT                                  = 6;   // 港股通
    static const uint8_t kFutureOption                         = 7;   // 期货期权
    static const uint8_t kCFETSRMB                             = 8;   // 银行间本币交易产品
    static const uint8_t kHKEx                                 = 9;   // 商业港股
    static const uint8_t kOthers                               = 255; // 其他
};

/**
 * @brief 日志输出级别定义
 */
class LogLevel
{
public:
    enum { kTrace = 0, kDebug = 1, kInfo  = 2, kWarn  = 3, kError = 4, kFatal = 5, };
};

/**
 * @brief 压缩格式类型定义
 */
class TcpCompressMode
{
public:
    static const uint8_t kNone  = 0;    // 不压缩
    static const uint8_t kHR    = 1;    // 华锐自定义压缩格式      
    static const uint8_t kZSTD  = 2;    // zstd压缩格式
};

/**
 * @brief 错误码定义
 */
class ErrorCode
{
public:

    /**
     * @brief 函数返回值定义，请结合输出日志
     */
    enum
    {
        kSuccess                               = 0,    // 成功
        kIllegalChannelMode                    = -1,   // 非法通道类型(参数channel_mode,参考ChannelMode枚举说明)
        kIllegalTcpCompressMode                = -2,   // 非法tcp压缩类型(参数tcp_compress_mode,参考TcpCompressMode枚举说明)
        kIllegalLogLevel                       = -3,   // 非法日志级别(参数min_log_level,参考LogLevel枚举说明)
        kIllegalOrderBookType                  = -4,   // 非法委托簿类型(参数enable_order_book,参考OrderBookType枚举说明)
        kOverLimitOBQueueSize                  = -5,   // 委托簿委托明细参数超过上限(参数order_queue_size,最高不能超过50)
        kIllegalHaMode                         = -6,   // 非法主备模式(参数ha_mode， 参考HighAvailableMode)
        kAbnormalUMSServerCnt                  = -7,   // 异常UMS服务数量(参数ums_server_cnt, 可设置范围为[1-8])
        kAuthServerSessionHBTimeOut            = -8,   // 鉴权服务端连接心跳超时(问题排查方向:客户端到UMS服务端中间TCP网络异常)
        kNoMarketPermission                    = -9,   // 账号没有对应的市场权限(需要启动的service需要的市场权限，账号没有开放)
        kThreadSetAffinityError                = -10,  // 线程设置亲密度失败,请确认传入的cpu_id 是否存在
        kNoEnoughCpuToSet                      = -11,  // 没有足够的cpu可用于设置线程亲密度
        kIllegalCpuListFormat                  = -12,  // cpu 列表格式错误(支持的格式有两种: "12,13,xxx" , "12-15, xxx")
        kIllegalIPAddress                      = -13,  // 非法IP地址
        kIllegalIPPort                         = -14,  // 非法IP端口
        kInternalError                         = -95,  // 内部错误(需要结合错误信息分析具体原因)
        kParamIllegal                          = -96,  // 非法Cfg配置参数(参考Cfg结构参数说明,已弃用)
        kNullSpi                               = -97,  // Init函数pSpi参数指针为空
        kUnInited                              = -98,  // 未调用Init初始化SDK环境
        kInited                                = -99,  // 重复初始化,单进程不支持重复调用Init
        kFailure                               = -100, // 失败
        
        kIllegalSubType                        = -101, // 非法订阅操作类型(参考SubscribeType枚举)
        kIllegalSubMarket                      = -102, // 非法订阅市场(参考MarketType枚举)
        kIllegalSubFlag                        = -103, // 非法权限订阅数据类型(参考SubscribeDataType枚举)
        kIllegalSubDataType                    = -104, // 非法品种订阅数据类型(参考SubscribeSecuDataType枚举)
        kIllegalSubCategory                    = -105, // 非法品种订阅品种类型(参考SubscribeCategoryType枚举)
        kAbnormalSubItemCnt                    = -106, // 异常订阅item数量(item 数量不能为0)
        
        kStaticDataRequestTooFrequently        = -201, // 静态数据请求太频繁(不支持高频循环请求静态数据)
        kQueryStaticFailed                     = -202, // 请求静态数据失败(请校验客户端和静态服务器的TCP是否连通)
        kNoMatchedStaticData                   = -203, // 服务端没有对应匹配的静态数据返回(请校验上游服务端对应的静态文件配置是否存在以及静态请求代码返回是否合理)
        kIllegalStaticQueryMarket              = -204, // 非法的静态数据请求市场类型(请校验请求信息的市场是否在支持范围内)
        kAbnormalQueryItemCnt                  = -205, // 异常查询item数量(item 数量不能为0)

        kIllegalDerivedSubType                 = -301, // 非法衍生数据订阅操作类型
        kIllegalDerivedSubMarket               = -302, // 非法衍生数据订阅市场
        kIllegalDerivedSubFlag                 = -303, // 非法委托簿订阅数据类型
        kIllegalDerivedSubDataType             = -304, // 非法衍生数据订阅数据类型
        kIllegalDerivedSubCode                 = -305, // 非法衍生数据订阅代码(代码不能为空)
        kAbnormalDerivedItemCnt                = -306, // 异常查询item数量(item 数量不能为0)
        kNoIOPVPermission                      = -307, // 账号无IOPV权限
        kNoOrderBookPermission                 = -308, // 账号无委托簿权限
        kDerivedNotLogin                       = -309, // 衍生数据为非登录状态(检查是否有AES登陆失败日志,查看具体的原因)
        kNoDerivedServer                       = -310, // 上游没有部署衍生数据加工AES服务端,请联系管理员确认
        kNoNeedOBSub                           = -311, // 非必要委托簿订阅(委托簿订阅只针对服务端下发委托簿模式生效,本地 & 不开启委托簿的情况下此订阅无意义)
        kOrderBookSubNumOverLimit              = -312, // 委托簿数据订阅数量超过账号上限
        kOrderBookSnapshotSubNumOverLimit      = -313, // 委托簿快照数据订阅数量超过账号上限
        kNoKLinePermission                     = -314, // 账号无K线权限

        kTCPChannelConnectFailed               = -401, // TCP通道连接失败，请确认客户端和上游组件TCP网络联通
        kTCPChannelSessionHBTimeout            = -402, // TCP通道连接心跳超时，请确认客户端和上游组件TCP网络联通
        kTCPChannelSendMessageFailed           = -403, // TCP通道发送消息失败，请确认客户端和上游组件TCP网络联通
        kTCPChannelComsumeTooSlow              = -404, // TCP通道消费过慢告警，下游消费速度跟不上上游数据发送的速度(请确认回调函数里面是否有进行耗时操作)
        kTCPChannelSessionClosed               = -405, // TCP通道连接断开(排查方向: 1.有kTCPChannelComsumeTooSlow 消费过慢告警日志,那有可能是消费速度过慢被上游主动断开连接 2.网络原因导致的连接断开)

        kUDPChannelConnectFailed               = -501, // UDP通道连接失败，请确认和上游组件TCP网络联通
        kUDPChannelSessionHBTimeout            = -502, // UDP通道连接心跳超时，请确认客户端和上游组件TCP网络联通
        kUDPChannelSendMessageFailed           = -503, // UDP通道TCP心跳保持&重传连接发送消息失败，请确认客户端和上游组件TCP网络联通
        kUDPChannelComsumeTooSlow              = -504, // UDP通道消费过慢告警，下游消费速度跟不上上游数据发送的速度(请确认回调函数里面是否有进行耗时操作)
        kUDPChannelSessionClosed               = -505, // UDP通道TCP心跳保持&重传连接断开(排查方向: 1.网络原因导致的连接断开)

        kSfOSnapComsumeTooSlow                 = -601, // Sf港交所委托挂单数据消费过慢(请确认回调函数里面是否有进行耗时操作)


        kAllocateMemoryFailed                   = -701, // 申请消息失败
        kCreateTcpClientFailed                  = -702, // 创建tcp client失败
        kQueryTimeout                           = -703, // 查询超时
        kQuerySendFailed                        = -704, // 查询请求发送失败
        kNoQueryPermission                      = -705, // 无查询权限
        kQueryEmptyData                         = -706, // 无数据
        kQueryPartData                          = -707, // 部分数据
        kOverReqestNumLimit                     = -708, // 超过查询请求数量限制
        kOverTickNumLimit                       = -709, // 超过查询区间长度限制
        kQueryLogonFailed                       = -710, // 查询登录失败
        kQueryConnectFailed                     = -711, // 查询连接失败
        kNoTickQueryServer                      = -712, // 上游没有部署逐笔查询服务端,请联系管理员确认
        kIllegalMarketType                      = -713, // 逐笔查询请求市场参数非法，目前只支持SZSE/SSE
        kIllegalChannelNo                       = -714, // 逐笔查询请求通道号参数非法，不能小于等于0
        kIllegalBeginSeqNum                     = -715, // 逐笔查询请求begin_appl_seq_num参数非法
        kNullQuerySpi                           = -716, // 查询spi为空，请检查
        kQueryEngineUnInited                    = -717, // 查询引擎未初始化（程序刚启动、中心切换、跨天均会初始化查询引擎）


        kNoADFPermission                       = -801, // 账号没有ADF二次开发框架权限
        kADFQueryTimeout                       = -802, // 查询ADF服务超时
        kMsgTypeNotSupport                     = -803, // msg_type类型不支持(检查cdd配置文件route节点是否配置正确或者检查ds是否配置了相关主题)
        kQueryFail                             = -804, // 查询失败，可查看CDD日志分析原因
        kNoCDDServer                           = -805, // 上游没有部署CDD服务端,请联系管理员确认

        kAccountNotExist                       = 1,    // 该账号不存在,请校验username是否正确填写
        kAccountOverConnectLimit               = 2,    // 操作账号保持连接上限(账号有保持连接的上限,默认上限为1,可以联系系统管理调整上限值)
        kNoLoginPermission                     = 3,    // 没有登陆实例权限,请确认中途是否是否修改账号UDF通道权限
        kTokenExpire                           = 4,    // 登陆token失效(跨周末运行时需要重置SDK,重新获取token)
        kInvalidLoginParameter                 = 5,    // 无效登陆参数(请校验channel_mode 是否正确填写)
        kLoginHandleTimeout                    = 6,    // 登陆请求处理超时(上游处理超时,请联系运维人员排查)
        kPwdError                              = 7,    // 账号密码错误
        kAuthServerConnectFailed               = 8,    // 连接UMS失败,请确认网络TCP连通
        kAccountExpire                         = 9,    // 账号过期
        kNotWhiteIpList                        = 10,   // 客户端ip 不在账号白名单内,请联系运维人员开修改白名单限制
        kNoAvaliableMode                       = 99,   // 账号权限没有channel_mode 参数对应的通道类型
        kUnknownError                          = 100,  // 未知错误
    };
};

/**
 * @brief 数据长度定义
 **/
class ConstField
{
public:
    static const uint32_t kIPMaxLen              = 24;       // IP地址的最大长度
    static const uint32_t kUsernameLen           = 32;       // 用户名的最大长度
    static const uint32_t kUMSItemLen            = 8;        // 服务项信息的最大个数
    static const uint32_t kChannelExternCfgLen   = 16;       // 通道扩展配置的最大个数
    static const uint32_t kPasswordLen           = 64;       // 用户名的最大长度
    static const uint32_t kSecurityCodeLen       = 16;       // 证券代码最大长度
    static const uint32_t kFutureSecurityCodeLen = 32;       // 期货证券代码最大长度
    static const uint32_t kSecurityNameLen       = 32;       // 证券名称最大长度
    static const uint32_t kPositionLevelLen      = 10;       // 行情档位
    static const uint32_t kHKExPositionLevelLen  = 40;       // 行情档位
    static const uint32_t kPathLen               = 255;      // 文件路径最大长度
    static const uint32_t kConfirmIdLen          = 8;        // 定价行情约定号 为空表示意向行情 否则为定价行情
    static const uint32_t kContactorLen          = 12;       // 联系人
    static const uint32_t kContactInfoLen        = 30;       // 联系方式
    static const uint32_t kTradingPhaseCodeLen   = 8;        // 交易状态标志
    static const uint32_t kContractIDLen         = 48;       // 合约交易代码
    static const uint32_t kContractSymbolLen     = 32;       // 期权合约简称
    static const uint32_t kUnderlyingTypeLen     = 3;        // 标的证券类型
    static const uint32_t kSecurityStatusFlagLen = 8;        // 期权合约状态信息标签
    static const uint32_t kExChangeInstIDLen     = 82;
    static const uint32_t kMaxMarketType         = 255;
    static const uint32_t kTradingStatusLen      = 8;
    static const uint32_t kSecurityStatusLen     = 8;
    static const uint32_t kMDStreamIDMaxLen      = 6;
    static const uint32_t kTypesLen = 8;
    static const uint32_t kDateLen = 10;
    static const uint32_t kTimeLen = 10;
    static const uint32_t kSecurityAbbreviationLen = 64;       // 证券简称最大长度
    static const uint32_t kSymbolLen = 128;                    // 证券简称最大长度（包括中文简称）
    static const uint32_t kMaxTypesLen = 16;                   // 证券类型最大长度
    static const uint32_t kMDStreamIDLen = 6;
    static const uint32_t kUnitName = 128;
    static const uint32_t kSubTradingPhaseLen = 8;      //债券细分交易阶段个数最大值
    static const uint32_t kQuoteIDLen = 10;             //债券逐笔报价消息编号长度
    static const uint32_t kMemberIDLen = 6;             //债券逐笔交易场代码长度
    static const uint32_t kInvestorTypeLen = 2;         //债券逐笔交易主体类型长度
    static const uint32_t kInvestorIDLen = 10;          //债券逐笔交易主体代码长度
    static const uint32_t kInvestorNameLen = 120;       //债券逐笔客户名称长度
    static const uint32_t kTraderCodeLen = 8;           //债券逐笔交易员代码程度
    static const uint32_t kSecondaryOrderIDLen = 16;    //债券逐笔竞买场次编号
    static const uint32_t kCodeTableSecurityStatusMaxLen = 16;      //代码表证券状态字符最大长度
    static const uint32_t KSecurityMarketLen = 4;                   //证券所属市场长度
    static const uint32_t kConsSecurityCodeLen = 32;                //成份证券代码长度
    static const uint32_t kSymbolETFLen = 128;                      //基金名称长度
    static const uint32_t kManagmentETFLen = 128;                   //基金公司名称长度
    static const uint32_t KUnderlyingSecurityIDSource = 4;          //拟合指数代码源长度
    static const uint32_t KUnderlyingSecurityID = 4;                //成分股所属市场ID长度
    static const uint32_t KReserved = 30;                           //预留字段长度
    static const uint32_t AllCashAmount = 12;                       //全现金替代的总金额长度
    static const uint32_t AllCashAremiumRate = 7;                   //全现金替代的申购溢价比例长度
    static const uint32_t AllCashDiscountRate = 7;                  //全现金替代的赎回折价比例长度
    static const uint16_t DefaultMaxOBSubNum = 500;                 //委托簿默认最大订阅数量
    static const uint16_t DefaultMaxOBSnapshotSubNum = 3000;        //委托簿快照默认最大订阅数量
    static const uint16_t DefaultMaxOBOrderQueueSize = 50;          //每一档委托队列数量上限
    static const uint16_t DefaultMaxOBEntrySize = 20;               //委托簿档位数量上限
    static const uint16_t DefaultMaxOBDeliverIntervalMs = 50;       //委托簿最小推送间隔
    static const uint16_t DefaultMinOBSnapshotDeliverIntervalMs = 10; //委托簿快照最小推送间隔
    static const uint16_t RegularShare = 9;                         //对应回购标准券长度
    static const uint32_t kTradePlaceLen = 3;                       //交易场所长度 
    static const uint32_t kNewFutureSecurityCodeLen = 82;           //期货非扩展代码长度(CTP期货行情合约需要扩展代码长度,其他的无需扩展)
    static const uint32_t kMarketCodeLen = 4;                       // 市场种类长度
    static const uint32_t kSpreadTableCodeLen = 2;                  // 价差代码长度

    //银行间
    static const uint32_t kCFETSSecurityCodeLen = 32;               // 银行间证券代码最大长度
    static const uint32_t kExecIDLen = 64;                          // 成交编号最大长度
    static const uint32_t kSecurityGroupLen = 32;                   // 交易品种最大长度
    static const uint32_t kBenchmarkCurveNameLen = 32;              // 参考利率最大长度
    static const uint32_t kUnderlyingSecurityCodeLen = 32;          // 标的券代码最大长度
    static const uint32_t kUnderlyingSymbolLen = 24;                // 标的券名称最大长度
    static const uint32_t kTradeLimitDaysLen = 16;                  // 交易期限最大长度
    static const uint32_t kMsgTypeLen = 6;                          // 报文类型最大长度
    static const uint32_t kTransactionMethodLen = 6;                // 成交方向最大长度
    static const uint32_t kMarketIndicatorLen = 4;                  // 市场标识最大长度
    static const uint32_t kSettlTypeLen = 8;                        // 清算速度最大长度
    static const uint32_t kExecTypeLen = 4;                         // 成交方式最大长度
    static const uint32_t kSecurityTypeLen = 12;                    // 证券类型最大长度
    static const uint32_t kSettlCurrencyLen = 4;                    // 币种长度
    static const uint32_t kPartyIDLen = 32;                         // 交易账户21位代码长度
    static const uint32_t kMatchTypeLen = 4;                        // 报价方式最大长度
    static const uint32_t kPositionLevel6 = 6;                      // 报价档位
    static const uint32_t kDireciveLevelLen = 5;                    // 行情档位     
    static const uint32_t kQuoteEntryIDLen = 64;                    // 报价编号最大长度
    static const uint32_t kAccountSubjectTypeLen = 4;               // 成交方式最大长度
    static const uint32_t kAccountIDLen = 8;                        // 交易账户6位码
    static const uint32_t kTraderIDLen = 64;                        // 交易员ID
    static const uint32_t kBrokerLen = 8;                           // 经纪公司
    static const uint32_t kMarketIDLen = 8;                         // 交易市场
    static const uint32_t kSecurityDescLen = 64;                    // 债券类型
    static const uint32_t kLatestSubjectRatingLen = 16;             // 评级
    static const uint32_t kYieldTypeLen = 32;                       // 收益率类型
    static const uint32_t kTextLen = 256;                           // 备注
    static const uint32_t kVolumeStringLen = 256;                   // 经纪报价最优行情交易量字符串长度
    
    static const uint32_t kDescLen = 48;                            // 债券类型长度
    static const uint32_t kSymLen = 111;                            // 工具简称长度
    static const uint32_t kIDLen = 51;                              // 工具代码长度
    static const uint32_t kRDISecurityCodeLen = 51;                 // RDI证券代码长度
    static const uint32_t kSecuritTypeCodeLen = 7;                  // 债券类型代码长度
    static const uint32_t kTermToMaturityStringLen = 16;            // 标准待偿期长度
    static const uint32_t kTermToMaturityLen = 48;                  // 待偿期长度
    static const uint32_t kIssuerCodeLen = 7;                       // 发行人代码长度
    static const uint32_t kCreditRatingAgencyLen = 101;             // 信用评级机构长度
    static const uint32_t kCrdRtgLen = 5;                           // 信用评级长度
    static const uint32_t kIssrLen = 121;                           // 发行机构简称中文长度
    static const uint32_t kEvntGrpLen = 7;                          // 行权类型长度
    static const uint32_t kCcyLen = 5;                              // 币种长度
    static const uint32_t kSecTypLen = 5;                           // 工具类型长度
    static const uint32_t kUndlyLen = 6;                            // 期限品种长度
    static const uint32_t kApplicableTrdgModelLen = 5;              // 适用交易方式长度
    static const uint32_t kDataLen = 48;                            // 日期长度

    // 港股
    static const uint32_t kDetailLen = 40;                          // 经纪明细数组长度
    static const uint32_t kBrokerNumLen = 5;                        // 席位号长度
    static const uint32_t kMarketLen = 3;                           // 市场类型
    static const uint32_t kDirLen = 3;                              // 交易方向长度
    static const uint32_t kOddLotOderLen = 20;                      // 碎股订单长度
    static const uint32_t kCurrencyCodeLen = 4;                     // 汇率代码长度

    // 美股
    static const uint32_t kUsaTradeStatusLen = 8;                   // 交易状态长度
    static const uint32_t kUsaExecTypeLen = 5;                      // 成交类型长度

    // 上海固收行情
    static const uint32_t kFixedIncomeOrderCodeLen = 11;            // 订单编号长度
    static const uint32_t kFixedIncomeQuoteIDLen = 16;              // 报价方长度
    // 上海固收流行情
    static const uint32_t kUnmatchQuoteIDLen = 18;                  //债券非匹配成交逐笔消息编号长度
    static const uint32_t kUnmatchMemberIDLen = 12;                 //债券非匹配成交逐笔报价方交易参与人机构代码长度
    static const uint32_t kUnmatchTraderCodeLen = 12;               //债券非匹配成交逐笔交易员代码长度
    static const uint32_t kUnmatchInvestorNameLen = 32;             //债券非匹配成交逐笔客户名称长度
    static const uint32_t kUnmatchSecondaryOrderIDLen = 18;         //债券非匹配成交逐笔竞买场次编号
    static const uint32_t kUnmatchBasketLen = 16;                   //债券非匹配成交逐笔消息编号长度

    // 申万指数
    static const uint32_t kSWSymbolMaxLen = 32;                     // 申万证券简称最大长度
};

/**
 * @brief 高可用类型
 */
class HighAvailableMode
{
public:
    enum 
    {
        kMasterSlaveA        = 0,                           // 主备切换模式的A模式,主通道异常的情况下,自动切换到备通道
        kMasterSlaveB        = 1,                           // 同kMasterSlaveA效果一致(兼容性考虑保留此类型)
        kRegularDataFilter   = 2                            // 规则数据过滤模式，该模式下对单独类型数据中的特征值做过滤，比如逐笔数据中根据通道号和逐笔序号执行数据过滤
    };
};

/**
 * @brief 通道类型
 */
class ChannelMode
{
public:
    static const uint64_t kRDMA = 0x00000001;               // RDMA 抓包方式获取数据
    static const uint64_t kEXA  = 0x00000002;               // EXA 抓包方式获取数据
    static const uint64_t kAMI  = 0x00000004;               // AMI 组播方式获取数据
    static const uint64_t kTCP  = 0x00000008;               // TCP 流方式获取数据
    static const uint64_t kPCAP = 0x00000010;               // Libpcap 抓包方式获取数据
    static const uint64_t kMDDP = 0x00000020;               // Mddp 组播方式获取数据
    static const uint64_t kFPGA = 0x00000040;               // FPGA 方式获取数据
    static const uint64_t kUDP =  0x00000080;               // AMD组播方式获取数据
};

/**
 * @brief 订阅操作类型
 */
class SubscribeType
{
public:
    enum
    {
        kSet,                                               // 重新设置订阅(先取消所有旧订阅信息, 然后增加新订阅信息)
        kAdd,                                               // 增加订阅
        kDel,                                               // 删除订阅
        kCancelAll                                          // 取消所有订阅
    };
};

/**
 * @brief 权限订阅数据类型
 */
class SubscribeDataType
{
public:
    static const uint64_t kNone                                 = 0x000000000000;    ///< 订阅全部数据
    static const uint64_t kSnapshot                             = 0x000000000001;    ///< 订阅现货快照数据
    static const uint64_t kTickExecution                        = 0x000000000002;    ///< 订阅逐笔成交数据
    static const uint64_t kTickOrder                            = 0x000000000004;    ///< 订阅逐笔委托数据
    static const uint64_t kOrderQueue                           = 0x000000000008;    ///< 订阅委托队列数据
    static const uint64_t kIndexSnapshot                        = 0x000000000010;    ///< 订阅指数快照数据
    static const uint64_t kFutureSnapshot                       = 0x000000000020;    ///< 订阅期货快照数据
    static const uint64_t kOptionSnapshot                       = 0x000000000040;    ///< 订阅期权快照数据   
    static const uint64_t kHKTSnapshot                          = 0x000000000080;    ///< 订阅港股通快照数据
    static const uint64_t kAfterHourFixedPriceSnapshot          = 0x000000000100;    ///< 订阅盘后定价快照数据
    static const uint64_t kAfterHourFixedPriceTickExecution     = 0x000000000400;    ///< 订阅上交所盘后定价逐笔成交数据
    static const uint64_t kCSIIndexSnapshot                     = 0x000000000800;    ///< 订阅中证指数快照数据
    static const uint64_t kNEEQSnapshot                         = 0x000000001000;    ///< 订阅北交所快照数据
    static const uint64_t kFundExpertSnapshot                   = 0x000000002000;    ///< 订阅基金通快照数据
    static const uint64_t kCFETSSnapshot                        = 0x000000004000;    ///< 订阅银行间CFETS快照
    static const uint64_t kCFETSTickExecution                   = 0x000000008000;    ///< 订阅银行间CFETS逐笔  
    static const uint64_t kHKExSnapshot                         = 0x000000010000;    ///< 订阅商业港股行情快照
    static const uint64_t kHKExTickExecution                    = 0x000000020000;    ///< 订阅商业港股行情逐笔
    static const uint64_t kCFETSPrivateMarket                   = 0x000000040000;    ///< 订阅银行间本币私有行情
    static const uint64_t kUsaSnapshot                          = 0x000000080000;    ///< 订阅美股快照行情
    static const uint64_t kFixedIncomeSnapshot                  = 0x000000100000;    ///< 订阅固定收益快照数据
    static const uint64_t kHKExConnectTurnoverSnapshot          = 0x000000200000;    ///< 订阅商业港股港股通成交额快照
    static const uint64_t kHKMarketStatus                       = 0x000000400000;    ///< 订阅港股通市场状态数据
    static const uint64_t kHKTRealtimeLimit                     = 0x000000800000;    ///< 订阅港股通实时额度数据
    static const uint64_t kOrderNums                            = 0x000001000000;    ///< 订阅委托笔数数据
    static const uint64_t kSWIndexSnapshot                      = 0x000002000000;    ///< 订阅申万指数快照数据


};

/**
 * @brief 品种订阅数据类型
 */
class SubscribeSecuDataType
{
public:
    static const uint64_t kNone                                 = 0x000000000000;    ///< 订阅全部证券数据类别
    static const uint64_t kSnapshot                             = 0x000000000001;    ///< 订阅所有快照数据类别(eg:包括SubscribeDataType::kSnapshot在内的所有快照数据类型)
    static const uint64_t kTickExecution                        = 0x000000000002;    ///< 订阅所有逐笔成交数据(eg:包括SubscribeDataType::kTickExecution在内的所有逐笔成交数据类型)
    static const uint64_t kTickOrder                            = 0x000000000004;    ///< 订阅所有逐笔委托数据(eg:包括SubscribeDataType::kTickOrder在内的所有逐笔委托数据类型)
    static const uint64_t kOrderQueue                           = 0x000000000008;    ///< 订阅委托队列数据
    static const uint64_t kOrderNums                            = 0x000000000010;    ///< 订阅委托笔数数据
};

/**
 * @brief 品种订阅品种类型
 */
class SubscribeCategoryType
{
public:
    static const uint64_t kNone                                 = 0x000000000000;    ///< 订阅全部证券品种类别
    static const uint64_t kStock                                = 0x000000000001;    ///< 订阅股票证券品种类别
    static const uint64_t kFund                                 = 0x000000000002;    ///< 订阅基金证券品种类别
    static const uint64_t kBond                                 = 0x000000000004;    ///< 订阅债券证券品种类别
    static const uint64_t kIndex                                = 0x000000000008;    ///< 订阅指数证券品种类别
    static const uint64_t kHKT                                  = 0x000000000010;    ///< 订阅港股通证券品种类别
    static const uint64_t kOption                               = 0x000000000020;    ///< 订阅期权证券品种类别
    static const uint64_t kFutureOption                         = 0x000000000040;    ///< 订阅期货/期货期权证券品种类别
    static const uint64_t kCFETSRMB                             = 0x000000000080;    ///< 订阅银行间本币交易产品品种类别
    static const uint64_t kHKEx                                 = 0x000000000100;    ///< 订阅商业港股证券品种类别
    static const uint64_t kOthers                               = 0x100000000000;    ///< 订阅其他证券品种类别
};

/**
 * @brief 衍生数据订阅数据类型
 */
class SubscribeDerivedDataType
{
public:
    static const uint32_t kOrderBook                            = 1;    ///< 订阅委托簿数据
    static const uint32_t kOrderBookSnapshot                    = 2;    ///< 订阅委托簿快照数据
    static const uint32_t kIOPVSnapshot                         = 3;    ///< 订阅IOPV快照数据
    static const uint32_t k1MinKLine                            = 4;    ///< 订阅分钟KLine
};

/**
 * @brief 委托簿订阅数据类型
 */
class SubscribeOrderBookDataType
{
public:
    static const uint64_t kNone                                 = 0x000000000000;    ///< 订阅全部委托簿数据类型(kOrderBook,kOrderBookSnapshot)
    static const uint64_t kOrderBook                            = 0x000000000001;    ///< 订阅委托簿数据
    static const uint64_t kOrderBookSnapshot                    = 0x000000000002;    ///< 订阅委托簿快照数据
    static const uint64_t kIOPVSnapshot                         = 0x000000000004;    ///< 订阅IOPV快照，暂未用到（订阅数限制用）
    static const uint64_t k1MinKLine                            = 0x000000000008;    ///< 订阅分钟KLine
};

class KlineType
{
public:
    static const uint8_t k1MinKLine                            = 1;    ///< 1分钟k线
};

/**
 * @brief 异步事件级别定义
 */
class EventLevel
{
public:
    enum
    {
        kInfo  = 1, ///< 普通事件
        kWarn  = 2, ///< 告警事件
        kError = 3  ///< 错误事件，比较严重，需要介入处理
    };
};

/**
 * @brief 华锐自定义trading_phase_code
 */
class TradingPhaseCode 
{
public:
    static const uint16_t kStart                      = 1;          //启动（开市前）
    static const uint16_t kOpenAuction915             = 2;          //开盘集合竞价[09:15-09:20)
    static const uint16_t kOpenAuction920             = 3;          //开盘集合竞价[09:20-09:25)
    static const uint16_t kOpenAuction925             = 4;          //开盘集合竞价[09:25-09:30)
    static const uint16_t kContTrading                = 5;          //连续竞价
    static const uint16_t kLunchBreak                 = 6;          //休市
    static const uint16_t kCloseAuction               = 7;          //收盘集合竞价
    static const uint16_t kEndOfTrade                 = 8;          //闭市
    static const uint16_t kVolatility                 = 9;          //波动性中断
    static const uint16_t kHalt                       = 10;         //停牌（含全天停牌和临时停牌）
    static const uint16_t kOther                      = 99;         //其他
};

/**
 * @brief 历史查询以及回放请求数据类型
 */


/**
 * @brief 历史查询以及请求任务状态
 */

/**
 * @brief 异步事件类型
 */
class EventCode
{
public:
    enum
    {
        /*--------------------------与UMS会话层事件信息------------------------------*/
        kUMSConnectSuccess                  = 1,                    // 连接UMS成功
        kUMSConnectFailed,                                          // 连接UMS失败
        kUMSLogonSuccess,                                           // 登录UMS成功
        kUMSLogonFailed,                                            // 登录UMS失败
        kUMSHeartbeatTimeout,                                       // 与UMS心跳超时

        /*--------------------------硬件加速通道相关事件-----------------------------*/
        kChannelRDMAInitSuccess,                                    // RDMA通道开启成功
        kChannelRDMAInitFailed,                                     // RDMA通道开启失败
        kChannelEXAInitSuccess,                                     // EXA通道开启成功
        kChannelEXAInitFailed,                                      // EXA通道开启失败
        kChannelPCAPInitSuccess,                                    // PCAP通道开启成功
        kChannelPCAPInitFailed,                                     // PCAP通道开启失败

        /*--------------------------CTP硬件加速相关事件-----------------------------*/
        kCTPDASStreamStart,                                         // 通道数据流开始
        kCTPDASStreamFinished,                                      // 通道数据流结束

        /*--------------------------SzseBinary硬件加速相关事件----------------------*/
        kSzseBinaryDASStreamStart,                                  // 通道数据流开始
        kSzseBinaryDASStreamFinished,                               // 通道数据流结束

        /*--------------------------SseLDDS硬件加速相关事件-------------------------*/
        kSseLDDSDASStreamStart,                                     // 通道数据流开始
        kSseLDDSDASStreamFinished,                                  // 通道数据流结束

        /*--------------------------AMI组播通道相关事件-----------------------------*/
        kChannelAMIInitSuccess,								        // AMI通道开启成功
        kChannelAMIInitFailed,								        // AMI通道开启失败

        /*--------------------------TCP通道相关事件---------------------------------*/
        kChannelTCPInitSuccess,								        // TCP通道开启成功
        kChannelTCPInitFailed,								        // TCP通道开启失败
        kChannelTCPConnectSuccess,								    // TCP通道连接成功
        kChannelTCPConnectFailed,								    // TCP通道连接失败
        kChannelTCPLogonSuccess,								    // TCP通道登录成功
        kChannelTCPLogonFailed,								        // TCP通道登录失败
        kChannelTCPSessionClosed,								    // TCP通道连接断开
        kChannelTCPHeartbeatTimeout,								// TCP通道会话心跳失败
        kChannelTCPMarketDataDegrade,                               // TCP通道行情数据降级
        kChannelTCPMarketDataUpgrade,                               // TCP通道行情数据升级

        /*--------------------------主备切换相关事件--------------------------------*/
        kSourceMasterSlaveChanged,                                  // 主备源切换

        /*--------------------------SFS 获取代码表相关事件--------------------------*/
        kSFSEngineInitSuccess,                                      // SFS 获取代码表模块初始化成功

        /*--------------------------AES 服务端委托薄相关事件------------------------*/
        kSzseOrderBookRejoinSuccess,                                    //深圳市场委托簿同步成功
        kSzseOrderBookRejoinFailed,                                     //深圳市场委托簿同步失败
        kSseOrderBookRejoinSuccess,                                     //上海市场委托簿同步成功
        kSseOrderBookRejoinFailed,                                      //上海市场委托簿同步失败
        kOrderBookRejonConnectFailed,                                   //连接上游aes组件失败
        kIncompleteData,                                                // 服务端委托簿以recover 方式重启，数据有可能不完整
        kOrderBookLogonSuccess,                                         // 服务端委托簿主动推送模式登陆成功(委托簿属于衍生数据，此信号后续会逐步弃用，用kDerivedDataLogonSuccess 信号代替)
        kOrderBookLogonFailed,                                          // 服务端委托簿主动推送模式登陆失败(委托簿属于衍生数据，此信号后续会逐步弃用，用kDerivedDataLogonFailed 信号代替)
        kDerivedDataLogonSuccess,                                       // 行情衍生数据服务端登陆成功
        kDerivedDataLogonFailed,                                        // 行情衍生数据服务端登陆失败       
        
        /*--------------------------FPGA通道相关事件-------------------------------*/
        kChannelFPGAInitSuccess,                                    // FPGA通道开启成功
        kChannelFPGAInitFailed,                                     // FPGA通道开启失败

        /*--------------------------UDP通道相关事件-------------------------*/
        kChannelUDPInitSuccess,								        // UDP通道开启成功
        kChannelUDPInitFailed,								        // UDP通道开启失败
        kChannelUDPConnectSuccess,								    // UDP通道连接成功
        kChannelUDPConnectFailed,								    // UDP通道连接失败
        kChannelUDPLogonSuccess,								    // UDP通道登录成功
        kChannelUDPLogonFailed,								        // UDP通道登录失败
        kChannelUDPSessionClosed,								    // UDP通道连接断开
        kChannelUDPHeartbeatTimeout,								// UDP通道会话心跳失败
        kChannelUDPMarketDataDegrade,                               // UDP通道行情数据降级
        kChannelUDPMarketDataUpgrade,                               // UDP通道行情数据升级

        /*--------------------------ADF通道相关事件-------------------------*/
        kChannelADFInitSuccess,								        // ADF通道开启成功
        kChannelADFInitFailed,								        // ADF通道开启失败
        kChannelADFConnectSuccess,								    // ADF通道连接成功
        kChannelADFConnectFailed,								    // ADF通道连接失败
        kChannelADFLogonSuccess,								    // ADF通道登录成功
        kChannelADFLogonFailed,								        // ADF通道登录失败
        kChannelADFSessionClosed,								    // ADF通道连接断开
        kChannelADFHeartbeatTimeout 								// ADF通道会话心跳失败
    };
};

// /**
//  * @brief 逐笔查询类型（仅上海市场有效）
//  */
// class QueryTickType
// {
// public:
//     static const uint32_t kNone                     = 0;    // 无
//     static const uint32_t kTickOrder                = 1;    // 逐笔委托
//     static const uint32_t kTickExecution            = 2;    // 逐笔成交
//     static const uint32_t kBondTick                 = 3;    // 债券逐笔
//     static const uint32_t kAfterHourTickExecution   = 4;    // 盘后固定交易价格逐笔成交
// };

}; // end of ama
}; // end of amd
#endif
