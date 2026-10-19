// code_generator_tag
// constants:f7efecb9dc2e6aa8eed433c797d5e05b types:4b1c6ac864222617122ef3765f571413 template:6706299d8d8aa91ed775924bbcc05934 code:88d46ef9cdb8416bc104f5ee1bda3ac0
/**
 * Copyright (c) 2018 Archforce Financial Technology.  All rights reserved. 
 * Redistribution and use in source and binary forms, with or without  modification, are not permitted.   
 * For more information about Archforce, welcome to archforce.cn.
 * 
 * @file atp_quant_constant.h
 * @author Archforce
 * @brief constants
 * 
 * @note 常量及类型定义
 */


#ifndef ATP_QUANT_CONSTANT_H_
#define ATP_QUANT_CONSTANT_H_

#include "atp_quant_export.h"
#include <stdint.h>
namespace atp
{
    namespace quant_api
    {
        namespace prop
        {
            static constexpr const char* kLogLevel = "log_level";                                   // 日志级别，非必填，默认值ATPLogLevel::kInfo
            static constexpr const char* kCommonLogPath = "common_log_path";                        // 业务日志路径，非必填，默认值空，表示当前可执行文件所在目录
            static constexpr const char* kIndicatorLogPath = "indicator_log_path";                  // 指标日志路径，非必填，默认值空，表示当前可执行文件所在目录
            static constexpr const char* kIoLogPath = "io_log_path";                                // IO日志路径，非必填，默认值空，表示当前可执行文件所在目录
            static constexpr const char* kEncryptSchema = "encrypt_schema";                         // 加密模式，非必填，默认值ATPEncryptModeConst::kTransMode
            static constexpr const char* kIsEnableLatency = "is_enable_latency";                    // 是否启用时延统计功能，非必填，默认值false
            static constexpr const char* kCallbackResourceMode = "callback_resource_mode";          // 回调线程资源模式，非必填，默认值ATPCallbackResourceModeConst::kLowLatencyMode
            static constexpr const char* kCallbackThreadMode = "callback_thread_mode";              // 回调线程模型，非必填，默认值ATPThreadModelConst::kShared-共享 （已废除）
            static constexpr const char* kGroupId = "group_id";                                     // 回调线程分组ID，非必填，默认值0 （已废除）
            static constexpr const char* kMinResidentMicro = "min_resident_micro";                  // 回调线程休眠时间，单位μs，非必填，默认值1μs （已废除）
            static constexpr const char* kRecevieThreadCpu = "recevie_thread_cpu";                  // 回调线程绑定CPU，非必填，默认值0xFF，表示不绑定
            static constexpr const char* kSendThreadCpu = "send_thread_cpu";                        // 发送线程绑定CPU，非必填，默认值0xFF，表示不绑定
            static constexpr const char* kIsTcpDirect = "is_tcp_direct";                            // 是否启用TCPDirect，非必填，默认值false
            static constexpr const char* kBindIpAddress = "bind_ip_address";                        // 绑定本地网卡地址，非必填，默认值空，表示不绑定
            static constexpr const char* kLocations = "locations";                                  // 网关地址，格式为"ip:port;ip:port"，必填
            static constexpr const char* kAgwUser = "agw_user";                                     // 网关用户名，非必填，默认值空，表示匿名登录模式
            static constexpr const char* kAgwPassword = "agw_password";                             // 网关用户密码，非必填，默认值空
            static constexpr const char* kRetransmitMode = "retransmit_mode";                       // 重传模式，非必填，默认值ATPRetransmitModeConst::kQuickMode（ATP3.2.1.1版本开始支持）
            static constexpr const char* kMultiChannelFlag = "multi_channel_flag";                  // 多通道自主订阅标志，非必填，默认值0xFF，表示以后台开关为准（ATP3.2.0版本开始支持）
            static constexpr const char* kConnectionProtocol = "connection_protocol";               // 协议类型，非必填，默认值ATPConnectionProtocolConst::kTCPProtocol
            static constexpr const char* kOrderWay = "order_way";                                   // 委托方式，必填
            static constexpr const char* kClientFeatureCode = "client_feature_code";                // 终端识别码，必填
            static constexpr const char* kFPGABindIpAddress = "fpga_bind_ip_address";               // FPGA模式下，绑定本地网卡地址，非必填，默认值空，表示不绑定
            static constexpr const char* kFPGACallbackResourceMode = "fpga_callback_resource_mode"; // FPGA模式下，回调线程资源模式，非必填，默认值ATPCallbackResourceModeConst::kLowLatencyMode
            static constexpr const char* kFPGARecevieThreadCpu = "fpga_recevie_thread_cpu";         // FPGA模式下，回调线程绑定CPU，非必填，默认值0xFF，表示不绑定
            static constexpr const char* kConnectionMode = "connection_mode";                       // 连接模式，非必填，默认值0, 0:直连模式 1:统一接入模式
        } // namespace prop

        /**
         * @brief 日志级别
         */ 
        typedef uint8_t ATPLogLevelType;
        struct QUANT_API ATPLogLevel
        {
            static const ATPLogLevelType kNone = 0;
            static const ATPLogLevelType kFatal = 1;
            static const ATPLogLevelType kError = 2;
            static const ATPLogLevelType kWarning = 3;
            static const ATPLogLevelType kInfo = 4;
            static const ATPLogLevelType kDebug = 5;
            static const ATPLogLevelType kVerbose = 6;
        };


        /**
         * @brief 错误码类型 
         **/ 
        typedef int32_t ATPErrorCodeType;
        struct QUANT_API ATPErrorCode
        {
			//成功 
            static const ATPErrorCodeType kSuccess = 0;    
			//失败 
            static const ATPErrorCodeType kFailure = -1;    
			//正在连接 
            static const ATPErrorCodeType kIsConnecting = -2;    
			//已连接，建议检查是否重复调用连接 
            static const ATPErrorCodeType kAlreadyConneted = -3;    
			//未连接 
            static const ATPErrorCodeType kNotConnected = -4;    
			//连接失败，建议检查ip和端口 
            static const ATPErrorCodeType kConnectFailed = -5;    
			//连接异常 
            static const ATPErrorCodeType kConnectionError = -6;    
			//等待连接关闭 
            static const ATPErrorCodeType kWaitForDisconnected = -7;    
			//会话不存在 
            static const ATPErrorCodeType kSessionNotExist = -101;    
			//会话创建中 
            static const ATPErrorCodeType kIsEstablishing = -102;    
			//会话已创建 
            static const ATPErrorCodeType kIsEstablished = -103;    
			//会话恢复中 
            static const ATPErrorCodeType kIsRecovering = -104;    
			//会话关闭中 
            static const ATPErrorCodeType kIsClosing = -105;    
			//会话创建失败 
            static const ATPErrorCodeType kEstablishFailure = -106;    
			//非法地址 
            static const ATPErrorCodeType kInvalidLocation = -107;    
			//地址为空 
            static const ATPErrorCodeType kNullLocation = -108;    
			//网关登录失败 
            static const ATPErrorCodeType kAgwLoginFail = -114;    
			//回报同步处理失败 
            static const ATPErrorCodeType kAgwSyncFail = -115;    
			//客户登录失败 
            static const ATPErrorCodeType kCustLoginFail = -116;    
			//不支持FPGA连接 
            static const ATPErrorCodeType kNotSupportFPGA = -117;    
			//动态库未加载 
            static const ATPErrorCodeType kSharedLibraryNotLoad = -1000;    
			//API对象未成功构造 
            static const ATPErrorCodeType kNotSuccessfullyConstructed = -1002;    
			//回调模式错误 
            static const ATPErrorCodeType kCallbackModeError = -1003;    
			//文件不存在 
            static const ATPErrorCodeType kFileNotExist = -1005;    
			//节点数据不存在 
            static const ATPErrorCodeType kNodeNotExits = -1006;    
			//加密模式错误 
            static const ATPErrorCodeType kEncryptSchemaError = -1007;    
			//打开文件失败 
            static const ATPErrorCodeType kOpenFileFail = -1008;    
			//动态库加载失败 
            static const ATPErrorCodeType kLoadSharedLibraryFail = -1009;    
			//登录模式错误 
            static const ATPErrorCodeType kLoginModeError = -1010;    
			//日志级别错误 
            static const ATPErrorCodeType kLogLevelError = -1011;    
			//不允许重复初始化 
            static const ATPErrorCodeType kDupInitIsNotAllowed = -1012;    
			//账户ID为空错误 
            static const ATPErrorCodeType kUserIdNullError = -1013;    
			//委托方式为空错误 
            static const ATPErrorCodeType kOrderWayNullError = -1014;    
			//终端识别码为空错误 
            static const ATPErrorCodeType kClientFeatureCodeNullError = -1015;    
			//分页查询异常 
            static const ATPErrorCodeType kPagingQueryException = -1016;    
			//公钥key文件不存在 
            static const ATPErrorCodeType kPublicKeyFileNotExist = -1017;    
			//初始化日志失败 
            static const ATPErrorCodeType kInitLogFail = -1018;    
			//基础字段检查不通过 
            static const ATPErrorCodeType kBasicFieldsError = -1019;    
			//资源已释放 
            static const ATPErrorCodeType kIsReleased = -1020;    
			//公钥key文件打开失败 
            static const ATPErrorCodeType kPublicKeyFileOpenFail = -1021;    
			//空指针异常 
            static const ATPErrorCodeType kNullPointerError = -1022;    
			//未进行静态初始化 
            static const ATPErrorCodeType kNotStaticInit = -1023;    
			//已调用静态停止函数 
            static const ATPErrorCodeType kIsStoped = -1024;    
			//未登录 
            static const ATPErrorCodeType kNotLogin = -1025;    
			//密码为空错误 
            static const ATPErrorCodeType kPasswordNullError = -1026;    
			//证券代码找不到本地缓存 
            static const ATPErrorCodeType kSecurityIdxNotExist = -1027;    
			//账户信息找不到本地缓存 
            static const ATPErrorCodeType kAccountIdxNotExist = -1028;    
			//该消息非本客户端创建 
            static const ATPErrorCodeType kMessageNotCreatedByClient = -1029;    
			//登出模式错误 
            static const ATPErrorCodeType kLogoutModeError = -1030;    
			//找不到用户 
            static const ATPErrorCodeType kLCannotFind = 1;    
			//禁止登录 
            static const ATPErrorCodeType kLForbidLogin = 2;    
			//重复登录 
            static const ATPErrorCodeType kLRepeatLogin = 3;    
			//密码错误 
            static const ATPErrorCodeType kLPwdError = 4;    
			//委托方式错误 
            static const ATPErrorCodeType kLEnterModeError = 5;    
			//未找到分区号 
            static const ATPErrorCodeType kLNoPartion = 6;    
			//未找到报盘交易单元 
            static const ATPErrorCodeType kLNoPbuID = 7;    
			//密码二次加密失败 
            static const ATPErrorCodeType kLPassWDReEncodeError = 8;    
			//消息转发失败 
            static const ATPErrorCodeType kLForwardError = 9;    
			//非ATP客户禁止登录系统 
            static const ATPErrorCodeType kLNotATPCustomer = 10;    
			//外部鉴权系统异常 
            static const ATPErrorCodeType kLExternalAuthenticationSystemError = 11;    
			//其他错误 
            static const ATPErrorCodeType kLOtherError = 99;    
			//未知业务类型 
            static const ATPErrorCodeType kQUnKnownBusinessType = 100001;    
			//无效的证券代码 
            static const ATPErrorCodeType kQInvalidSecurityID = 100002;    
			//交易方向错误 
            static const ATPErrorCodeType kQInvalidSide = 100003;    
			//价格小于0 
            static const ATPErrorCodeType kQPriceLessThanZero = 100004;    
			//价格小于价格档位 
            static const ATPErrorCodeType kQPriceLessThanUnit = 100005;    
			//单位数量价格小于等于0 
            static const ATPErrorCodeType kQUnitPriceLessEqualZero = 100006;    
			//无效的资金帐号 
            static const ATPErrorCodeType kQInvalidFundId = 100007;    
			//未找到指定的订单 
            static const ATPErrorCodeType kQNotFoundOrder = 100008;    
			//未找到指定的成交回报 
            static const ATPErrorCodeType kQNotFoundTER = 100009;    
			//未找到股份 
            static const ATPErrorCodeType kQNotFoundShare = 100010;    
			//账户无此持仓 
            static const ATPErrorCodeType kQSharePositionNotExist = 100011;    
			//账户证券持仓不足一手 
            static const ATPErrorCodeType kQSharePositionInsufficient = 100012;    
			//账户可用资金不足一手 
            static const ATPErrorCodeType kQFundInsufficient = 100013;    
			//没找到股份交易规则 
            static const ATPErrorCodeType kQShareTradeRuleNotExist = 100014;    
			//证券基础数据错误 
            static const ATPErrorCodeType kQShareDataError = 100015;    
			//资金交易规则不存在 
            static const ATPErrorCodeType kQFundTradeRuleNotExist = 100016;    
			//证券账户不存在 
            static const ATPErrorCodeType kQInvalidAccountID = 100017;    
			//客户号在客户信息表中不存在 
            static const ATPErrorCodeType kQCustIDNonExist = 100018;    
			//客户密码错误 
            static const ATPErrorCodeType kQCustPWDError = 100019;    
			//客户号未启用 
            static const ATPErrorCodeType kQCustIDNotEnable = 100020;    
			//ETF申购超最大现金替代比例 
            static const ATPErrorCodeType kQETFCashComponentOverRange = 100021;    
			//未找到etf的成分股基础证券信息 
            static const ATPErrorCodeType kQNotFoundETFComponentSecurity = 100022;    
			//获取业务参数失败 
            static const ATPErrorCodeType kQNotFoundBizParam = 100023;    
			//获取质押券对应的标准券总量失败 
            static const ATPErrorCodeType kQNotFoundStandardShare = 100024;    
			//未找到客户债券托管量 
            static const ATPErrorCodeType kQNotFoundBondAssets = 100025;    
			//未找到客户净资产 
            static const ATPErrorCodeType kQNotFoundNetAssets = 100026;    
			//资金帐号不存在 
            static const ATPErrorCodeType kQFundAccountIDNotExist = 100027;    
			//价格错误 
            static const ATPErrorCodeType kQOrderPriceRangeOut = 100028;    
			//港股转托管委托不能同时选择证券编码和权益编码 
            static const ATPErrorCodeType kQHKDTIDBothInsufficient = 100029;    
			//港股转托管已经做过全部转托管不能再做转托管 
            static const ATPErrorCodeType kQHKDTHaveAll = 100030;    
			//融资融券合约明细不存在 
            static const ATPErrorCodeType kQNotFoundContractSpecifications = 100032;    
			//资金专户不存在 
            static const ATPErrorCodeType kQNotFoundSpecialAccount = 100033;    
			//信用资料不存在 
            static const ATPErrorCodeType kQNotFoundCreditInfo = 100034;    
			//信用额度不存在 
            static const ATPErrorCodeType kQNotFoundCreditLimit = 100035;    
			//计算保证金余额时发生错误 
            static const ATPErrorCodeType kQErrorOccursOnCalcMarginBalance = 100036;    
			//融券专户不存在 
            static const ATPErrorCodeType kQNoSecuritySpecialAccount = 100037;    
			//未找到融资融券股份负债 
            static const ATPErrorCodeType kQNotFoundShareLiability = 100038;    
			//回购业务参数表中的入库集中度上限字段为0或质押出入库业务规则表中的上海市场信用债总发行量为0 
            static const ATPErrorCodeType kQBizParamInvalid = 100039;    
			//集中度检查不通过 
            static const ATPErrorCodeType kQPledgeCentrationNoPassed = 100040;    
			//找不到资金交易规则模板 
            static const ATPErrorCodeType kQFundTradeTemplateNotExist = 100042;    
			//融资保证金比例不存在 
            static const ATPErrorCodeType kQFinanceGuaranteeRatioNotExist = 100043;    
			//融券保证金比例不存在 
            static const ATPErrorCodeType kQSecurityGuaranteeRatioNotExist = 100044;    
			//该证券在标的清单中不存在 
            static const ATPErrorCodeType kQCrdUnderlyingNonExist = 100045;    
			//该证券非担保品标的 
            static const ATPErrorCodeType kQNotCollateral = 100046;    
			//未找到融资融券资金负债 
            static const ATPErrorCodeType kQNotFoundFundLiability = 100047;    
			//维持担保比例不存在 
            static const ATPErrorCodeType kQMntGuaranteeRatioNotExist = 100048;    
			//没有可用余券 
            static const ATPErrorCodeType kQNotAvailExtraSecurity = 100049;    
			//非融资标的 
            static const ATPErrorCodeType kQNotCrdBuyUnderlying = 100050;    
			//非融券标的 
            static const ATPErrorCodeType kQNotCrdSellUnderlying = 100051;    
			//卖数量单位小于等于0 
            static const ATPErrorCodeType kQSellQtyUnitLessEqualZero = 100052;    
			//两融专户禁止该交易业务 
            static const ATPErrorCodeType kQSpecialAccountNotAllowTheBusiness = 100053;    
			//融资保底额度信息不存在 
            static const ATPErrorCodeType kQBuyPrivateAmountNotFound = 100054;    
			//融券保底额度信息不存在 
            static const ATPErrorCodeType kQSellPrivateAmountNotFound = 100055;    
			//融资专户禁止该交易业务 
            static const ATPErrorCodeType kQFinanSpecialAccountNotAllowTheBusiness = 100056;    
			//未获取到汇率信息 
            static const ATPErrorCodeType kQGetCustFeeConfigFail = 100057;    
			//资金账号不支持该币种 
            static const ATPErrorCodeType kQFundNotSupportCurrency = 100058;    
			//证券类型对应的证券品种不存在 
            static const ATPErrorCodeType kQGetSecurityVarietyFail = 100059;    
			//客户费用信息不存在 
            static const ATPErrorCodeType kQFundFeeInfoFail = 100060;    
			//无行情数据 
            static const ATPErrorCodeType kQNoQuoteInfo = 100061;    
			//申报数量不合法，必须为正整数 
            static const ATPErrorCodeType kQDeclaredNumNotInt = 100062;    
			//申报数量大于最大可委托数 
            static const ATPErrorCodeType kQDeclaredNumGreaterThanMax = 100063;    
			//获取价格失败 
            static const ATPErrorCodeType kQGetPriceFailed = 100064;    
			//融资融券未知三方系统类型 
            static const ATPErrorCodeType kQUnknownThirdSystemType = 100065;    
			//交易密码解密失败 
            static const ATPErrorCodeType kQPasswordDecryptFail = 100066;    
			//发送外部系统失败 
            static const ATPErrorCodeType kQSendThirdPartyFail = 100067;    
			//外部系统拒绝 
            static const ATPErrorCodeType kQThirdPartyReject = 100068;    
			//新股额度为零 
            static const ATPErrorCodeType kQIPOIsZero = 100069;    
			//无效指定偿还合约 
            static const ATPErrorCodeType kQDesignateContractInvalid = 100070;    
			//当前还券类型不支持偿还指定合约 
            static const ATPErrorCodeType kQDesignateContractRedeliverTypeDeny = 100071;    
			//期权合并行权认沽行权价格必须高于认购行权价格 
            static const ATPErrorCodeType kQComninationPutExPriceMustUpCallExPrice = 100072;    
			//期权合并行权腿认沽认购类型不能相同 
            static const ATPErrorCodeType kQCombinationLegCallOrPutConsistent = 100073;    
			//成分合约单位不相等 
            static const ATPErrorCodeType kQCombinationContractUnitDiff = 100074;    
			//期权行权操作不在规定的行权日期范围内 
            static const ATPErrorCodeType kQOptionExerciseOverDateRange = 100075;    
			//指定合约偿还未配置指定合约偿还参数 
            static const ATPErrorCodeType kQDesignateContractRepayControlNotSet = 100076;    
			//系统繁忙 
            static const ATPErrorCodeType kQSystemBusy = 100077;    
			//成交查询条件入参非法 
            static const ATPErrorCodeType kQTradeOrderQueryConditionIllegal = 100078;    
			//TE不存在此网关用户信息 
            static const ATPErrorCodeType kQAgwUserNotExistsInTE = 100079;    
			//查询消息过大 
            static const ATPErrorCodeType kQMessageOutOfPayload = 100080;    
			//触发风控 
            static const ATPErrorCodeType kQTriggerRules = 100081;    
			//未启用超极速报盘模式 
            static const ATPErrorCodeType kExPluginModeNoEnable = 100082;    
			//证券类型不正确 
            static const ATPErrorCodeType kQInvalidSecurityType = 100083;    
			//持有股份不足 
            static const ATPErrorCodeType kQShareInsufficient = 100084;    
			//查询条件不满足业务要求 
            static const ATPErrorCodeType kQQueryConditionNotMatchBiz = 100085;    
			//港股通公司行为代码不存在 
            static const ATPErrorCodeType kQHkCorporateActionIDNotExist = 100086;    
			//港股通投票证券代码与其对应的公告编号或投票议案不存在 
            static const ATPErrorCodeType kQHkVotingNoNonExist = 100087;    
			//联动市场证券账户不存在 
            static const ATPErrorCodeType kQRelatedAccountNotExists = 100094;    
			//联动市场证券账户信息不完整 
            static const ATPErrorCodeType kQRelatedAccountMissed = 100095;    
			//联动市场证券账户存在多个 
            static const ATPErrorCodeType kQRelatedAccountIsMultiple = 100096;    
			//联动市场证券账户状态异常 
            static const ATPErrorCodeType kQRelatedAccountStatusAbnormal = 100097;    
			//发送联动市场消息失败，请检查网络或组件是否正常 
            static const ATPErrorCodeType kQSendRelatedMessagesFailed = 100098;    
			//该ETF的申赎类型为全实物申赎，当前API版本不支持 
            static const ATPErrorCodeType kQNotSupportCMTETFAllPhysicalPR = 100099;    
			//查询的数量不为交易数量单位的整数倍 
            static const ATPErrorCodeType kQQueryQtyStallError = 100100;    
			//查询的数量超过数量上下限 
            static const ATPErrorCodeType kQQueryQtyRangeOut = 100101;    
			//转出资金TE找不到资金账户 
            static const ATPErrorCodeType kEFundOutTEFundIDNonExist = 200001;    
			//找不到资金转出证券账户 
            static const ATPErrorCodeType kEFundOutTEAccountIDNonExist = 200002;    
			//找不到资金转入证券账户 
            static const ATPErrorCodeType kEFundOutTEOtherAccountIDNonExist = 200003;    
			//内部划拨时转出资金不足 
            static const ATPErrorCodeType kEFundOutTEInsufficient = 200004;    
			//转入资金TE找不到资金账户 
            static const ATPErrorCodeType kEFundInTEFundIDNonExist = 200005;    
			//转入资金TE找不到证券账户 
            static const ATPErrorCodeType kEFundInTEAccountIDNonExist = 200006;    
			//资金转入TE资金增加后超过资金数值上限 
            static const ATPErrorCodeType kEFundInTEAmountOver = 200007;    
			//密码错误 
            static const ATPErrorCodeType kEPWDError = 200008;    
			//客户号不存在 
            static const ATPErrorCodeType kECustIDNonExist = 200009;    
			//客户号不可用 
            static const ATPErrorCodeType kECustIDNotAvailable = 200010;    
			//转入转出账号相同 
            static const ATPErrorCodeType kESameTransferAccount = 200011;    
			//找不到资金转入证券账户 
            static const ATPErrorCodeType kEFundInTEOtherAccountIDNonExist = 200012;    
			//资金转出TE回滚后，资金增加后超过资金数值上限 
            static const ATPErrorCodeType kEFundOutTEAmountOver = 200013;    
			//外部指令重复 
            static const ATPErrorCodeType kEExternalInsRepeat = 200014;    
			//转出资金小于等于0 
            static const ATPErrorCodeType kEFundInValueTooSmall = 200015;    
			//转入转出证券账号的TE分区相同 
            static const ATPErrorCodeType kESameTransferAccountPartionNO = 200016;    
			//客户没有划拨资金的权限 
            static const ATPErrorCodeType kENoTransFundRight = 200017;    
			//资金账户与转出证券账号不匹配 
            static const ATPErrorCodeType kEFundIDNotMatchOutAccountID = 200018;    
			//资金账户与转入证券账号不匹配 
            static const ATPErrorCodeType kEFundIDNotMatchInAccountID = 200019;    
			//外部指令非法 
            static const ATPErrorCodeType kEExternalInsIllegal = 200020;    
			//维持担保比例低于提取线 
            static const ATPErrorCodeType kELowerThanExtractLine = 200021;    
			//不满足特殊维保比例检查项 
            static const ATPErrorCodeType kELowerThanSpecExtractLine = 200022;    
			//信用资料客户信息不存在 
            static const ATPErrorCodeType kECreditInfoNotFound = 200023;    
			//集中度超限 
            static const ATPErrorCodeType kEConcentrationRatioExceedLimit = 200024;    
			//多头集中度超限 
            static const ATPErrorCodeType kENetLongConcentrationRatioExceedLimit = 200025;    
			//空头集中度超限 
            static const ATPErrorCodeType kENetShortConcentrationRatioExceedLimit = 200026;    
			//标的证券类别或风险等级不存在 
            static const ATPErrorCodeType kEUnderlyingTypeorRiskLevelNotExist = 200027;    
			//证券编码与市场编码在交易信息表中不存在 
            static const ATPErrorCodeType kESecurityIDNonExist = 200028;    
			//融资保底额度信息不存在 
            static const ATPErrorCodeType kEBuyPrivateAmountNotFound = 200029;    
			//融资保底额度不足 
            static const ATPErrorCodeType kEBuyPrivateAmountInsufficient = 200030;    
			//融资专户不存在 
            static const ATPErrorCodeType kENoMarginTradingSpecialAccount = 200031;    
			//专户可划拨额度不足 
            static const ATPErrorCodeType kESpecialAccountAmountInsufficient = 200032;    
			//调减后的额度超过上下限 
            static const ATPErrorCodeType kESubResultOverRange = 200033;    
			//信用额度信息不存在 
            static const ATPErrorCodeType kECreditAmountNotFound = 200034;    
			//信用额度不足禁止调减 
            static const ATPErrorCodeType kECreditAmountRequired = 200035;    
			//融资信用额度不足禁止调减 
            static const ATPErrorCodeType kECreditFinancingAmountRequired = 200036;    
			//融券信用额度不足禁止调减 
            static const ATPErrorCodeType kECreditSecurityAmountRequired = 200037;    
			//划拨额度金额不能小于0或者同时为0 
            static const ATPErrorCodeType kECreditAmountValueError = 200038;    
			//不支持的划拨指令 
            static const ATPErrorCodeType kEUnsupportedFundTransfer = 200039;    
			//FPGA更新资金失败 
            static const ATPErrorCodeType kEFpgaUpdateFundFailed = 200040;    
			//转出资金账户与投资组合不匹配 
            static const ATPErrorCodeType kEFundOutFundAccountIDNotMatchShareUnit = 200045;    
			//转入资金账户与投资组合不匹配 
            static const ATPErrorCodeType kEFundInFundAccountIDNotMatchShareUnit = 200046;    
			//找不到转出投资组合 
            static const ATPErrorCodeType kEFundOutShareUnitNonExist = 200047;    
			//找不到转入投资组合 
            static const ATPErrorCodeType kEFundInShareUnitNonExist = 200048;    
			//持仓调整的数量超过上下限制 
            static const ATPErrorCodeType kEAdjustQtyOverRange = 200049;    
			//持仓调增后超过上限 
            static const ATPErrorCodeType kEAdjustQtyResultOverRange = 200050;    
			//持仓调减后小于0 
            static const ATPErrorCodeType kEAdjustQtyResultLessThanZero = 200051;    
			//未知的调整类型 
            static const ATPErrorCodeType kEUnknowTransferType = 200052;    
			//无效的用户持仓信息 
            static const ATPErrorCodeType kEUnknowAccountContractShareInfo = 200053;    
			//划出方持仓不足 
            static const ATPErrorCodeType kEShareTransferOutShareNotEnough = 200054;    
			//未知的资金账号 
            static const ATPErrorCodeType kEUnknowFundAccountID = 200055;    
			//临时调增金额小于等于0 
            static const ATPErrorCodeType kETempAddLessThanZero = 200056;    
			//临时调减金额小于等于0 
            static const ATPErrorCodeType kETempSubLessThanZero = 200057;    
			//找不到投资组合 
            static const ATPErrorCodeType kEShareUnitNonExist = 200058;    
			//不允许相同投资组合间划转 
            static const ATPErrorCodeType kEShareTransferTheSame = 200059;    
			//资金账户与投资组合不匹配 
            static const ATPErrorCodeType kEFundAccountIDNotMatchShareUnit = 200060;    
			//调整的数值超过上下限 
            static const ATPErrorCodeType kEAdjustValueOverRange = 200061;    
			//临时调增后的金额超过上下限，N18(4) 
            static const ATPErrorCodeType kETempAddResultOverRange = 200062;    
			//临时调减后的金额超过上下限，N18(4) 
            static const ATPErrorCodeType kETempSubResultOverRange = 200063;    
			//未知的证券账号 
            static const ATPErrorCodeType kEUnknowAccountID = 200064;    
			//临时冻结后的可用数量小于0 
            static const ATPErrorCodeType kEFrozenQtyResultLessThanZero = 200065;    
			//临时解冻后的冻结数量小于0 
            static const ATPErrorCodeType kEThawQtyResultLessThanZero = 200066;    
			//找不到持仓转出证券账户 
            static const ATPErrorCodeType kEShareOutTEAccountIDNonExist = 200067;    
			//找不到持仓转入证券账户 
            static const ATPErrorCodeType kEShareOutTEOtherAccountIDNonExist = 200068;    
			//不支持的币种 
            static const ATPErrorCodeType kECurrencyNotSupport = 200069;    
			//不同报盘证券账户之间不允许划拨 
            static const ATPErrorCodeType kEShareTransferTheSameOfferAccountID = 200070;    
			//证券编码与市场编码在交易信息表中不存在 
            static const ATPErrorCodeType kRSecurityIDNonExist = 300001;    
			//业务开关被关闭 
            static const ATPErrorCodeType kRSecurityBusinessSwitchClose = 300002;    
			//交易单元权限不存在 
            static const ATPErrorCodeType kRPbuBusinessPermissionNonExist = 300003;    
			//账户信息表中不存在账户 
            static const ATPErrorCodeType kREffectiveContractAccountNonExist = 300004;    
			//在发行业务不允许重复认购的情况下，做重复认购 
            static const ATPErrorCodeType kRIssueDuplicate = 300005;    
			//证券账户对应业务产品交易被限制 
            static const ATPErrorCodeType kRInvestorLimitTradeLimit = 300006;    
			//委托价格在价格涨跌幅范围之外 
            static const ATPErrorCodeType kROrderPriceRangeOut = 300007;    
			//委托价格不为价格档位的整数倍 
            static const ATPErrorCodeType kROrderPriceStallError = 300008;    
			//委托数量超过数量上下限 
            static const ATPErrorCodeType kROrderQtyRangeOut = 300009;    
			//委托数量不为交易数量单位的整数倍 
            static const ATPErrorCodeType kROrderQtyStallError = 300010;    
			//卖开仓保证金不足 
            static const ATPErrorCodeType kRSellOpenFundInsufficient = 300011;    
			//买开仓权利金不足 
            static const ATPErrorCodeType kRBuyOpenFundInsufficient = 300012;    
			//账户可用资金小于当前交易金额+交易费用 
            static const ATPErrorCodeType kRTransactionCostInsufficient = 300013;    
			//客户标的合约持仓超过限额 
            static const ATPErrorCodeType kRInvestorContractPositionLimitOut = 300014;    
			//券商标的合约持仓超过限额 
            static const ATPErrorCodeType kRMemberContractPositionLimitOut = 300015;    
			//客户标的合约交易量超过限额 
            static const ATPErrorCodeType kRInvestorContractAmountLimitOut = 300016;    
			//券商标的合约交易量超过限额 
            static const ATPErrorCodeType kRMemberContractAmountLimitOut = 300017;    
			//平仓可平量不足 
            static const ATPErrorCodeType kRClosePositionInsufficient = 300018;    
			//备兑开仓标的券股份不足 
            static const ATPErrorCodeType kRCoverOpenPositionShareInsufficient = 300019;    
			//当前市场状态不允许申报委托 
            static const ATPErrorCodeType kRMarketNotOpen = 300020;    
			//营业部无对应业务权限 
            static const ATPErrorCodeType kRBranchBusinessPermissionNonExist = 300021;    
			//订单无法被撤销 
            static const ATPErrorCodeType kRUnableToCancelled = 300022;    
			//证券账户无对应业务产品权限 
            static const ATPErrorCodeType kRInvestorGradedNotEnough = 300023;    
			//期权买入开仓超过客户买入额度限制 
            static const ATPErrorCodeType kRBuyOpenFundOver = 300024;    
			//证券账户业务和产品无权限 
            static const ATPErrorCodeType kRAccountBusinessPermissionNonPrivileged = 300025;    
			//证券账户在证券账户信息表中不存在 
            static const ATPErrorCodeType kRAccountIDNonExist = 300026;    
			//账户可用资金小于当前交易金额 
            static const ATPErrorCodeType kRFundInsufficient = 300027;    
			//账户持有股份不足 
            static const ATPErrorCodeType kRShareInsufficient = 300028;    
			//发行申购证券与账户所属市场不一致 
            static const ATPErrorCodeType kRSecurityMarketInconsistency = 300029;    
			//投资者的业务被黑名单限制 
            static const ATPErrorCodeType kRBusinessForbid = 300030;    
			//投资者的业务不在白名单中 
            static const ATPErrorCodeType kRBusinessWhiteListNonExist = 300031;    
			//合格投资者身份信息不存在 
            static const ATPErrorCodeType kRAccountQualificationClassNotFound = 300032;    
			//合格投资者身份不匹配 
            static const ATPErrorCodeType kRQualificationClassOut = 300033;    
			//超过投资者认购额度限制 
            static const ATPErrorCodeType kRInvestorSubscriptionQuotaTradeLimit = 300034;    
			//投资者认购额度表中不存在 
            static const ATPErrorCodeType kRInvestorSubscriptionQuotaNonExist = 300035;    
			//交易单元业务权限不足 
            static const ATPErrorCodeType kRPbuBusinessNonPrivileged = 300036;    
			//转托管交易单元的转入和转出不能一致 
            static const ATPErrorCodeType kRTransfereePBUIDSameAsPBUID = 300037;    
			//证券业务开关表中不存在 
            static const ATPErrorCodeType kRSecurityBusinessSwitchNonExist = 300038;    
			//证券账户无该类证券交易权限 
            static const ATPErrorCodeType kRAccountBusinessPermissionNonExist = 300039;    
			//账户当前的状态不允许做指定交易 
            static const ATPErrorCodeType kRDesignatedTransactionDenied = 300040;    
			//账户当前的状态不允许做撤销指定交易 
            static const ATPErrorCodeType kRCancelDesignatedTransactionDenied = 300041;    
			//指定交易证券ID错误 
            static const ATPErrorCodeType kRDesignatedTransactionSecurityIDError = 300042;    
			//账户未指定交易单元 
            static const ATPErrorCodeType kRUnassignedPBU = 300043;    
			//交易单元证券权限不足 
            static const ATPErrorCodeType kRPbuSecurityNonPrivileged = 300044;    
			//业务不支持当前的交易指令 
            static const ATPErrorCodeType kRBusinessUnsupportedInstruction = 300045;    
			//该指令消息找不到对应的原始订单 
            static const ATPErrorCodeType kROriginOrderNotExist = 300046;    
			//证券账户与资金账户不匹配 
            static const ATPErrorCodeType kRAccountIDNotMatchedFundAccountID = 300047;    
			//不允许市价指令 
            static const ATPErrorCodeType kRMarketPriceNotAllowed = 300048;    
			//市场不允许该业务 
            static const ATPErrorCodeType kRBusinessMarketTradingPeriodNotExist = 300049;    
			//期权行权操作不在规定的行权日期范围内 
            static const ATPErrorCodeType kROptionExerciseOverDateRange = 300050;    
			//指定的交易单元不在交易单元信息表中 
            static const ATPErrorCodeType kRDesignatedTransactionPBUInvalid = 300051;    
			//当前证券账户状态不允许交易 
            static const ATPErrorCodeType kRAccountStatusInvalid = 300052;    
			//权利仓的净持仓量不足以行权 
            static const ATPErrorCodeType kROptionExerciseBuyOpenQtyInsufficient = 300053;    
			//停牌证券不允许交易 
            static const ATPErrorCodeType kRSecurityStatusSuspend = 300054;    
			//客户未开通买入风险警示板证券的权限 
            static const ATPErrorCodeType kRAccountSecurityStatusNonPrivileged = 300055;    
			//交易所已闭市 
            static const ATPErrorCodeType kRExchageHasBeenClose = 300056;    
			//当日买入的风险警示板证券数量超过上限 
            static const ATPErrorCodeType kRBuyRiskWarningOverRange = 300057;    
			//无法分配申报合同号 
            static const ATPErrorCodeType kRUnableToAllocClOrdID = 300058;    
			//不在引擎接受订单的时间范围内 
            static const ATPErrorCodeType kRNotInTEAcceptOrderPeriod = 300059;    
			//当前时间禁止撤单 
            static const ATPErrorCodeType kRInRejectCancelOrderPeriod = 300060;    
			//可转托管的证券余量不足 
            static const ATPErrorCodeType kRDesignationSecurityInsufficient = 300061;    
			//找不到订单对应的资金交易规则模板 
            static const ATPErrorCodeType kRFundTradeTemplateNotExist = 300062;    
			//当前证券代码不允许申购指令 
            static const ATPErrorCodeType kRPurchaseNotAllowed = 300063;    
			//当前证券代码不允许赎回指令 
            static const ATPErrorCodeType kRRedemptionNotAllowed = 300064;    
			//单个账户ETF委托数量超过账户累计申赎总额限制 
            static const ATPErrorCodeType kRPurchaseRedemptionLimitOut = 300065;    
			//单个账户ETF委托数量超过账户净申赎总额限制 
            static const ATPErrorCodeType kRNetPurchaseRedemptionLimitOut = 300066;    
			//配股配债时订单输入证券代码需为配售代码 
            static const ATPErrorCodeType kRSecurityIsNotRightsIssueWarrant = 300067;    
			//当前日期非配售日期 
            static const ATPErrorCodeType kRBusinessDateNotAllowed = 300068;    
			//ETF申赎不允许撤单 
            static const ATPErrorCodeType kRETFPurRedCancelNotAllowed = 300069;    
			//ETF申购超过最大现金替代比例 
            static const ATPErrorCodeType kRETFCashComponentOverRange = 300070;    
			//找不到对应的报盘通道 
            static const ATPErrorCodeType kRCannotFindOfferWay = 300071;    
			//该证券不在认购期内 
            static const ATPErrorCodeType kRBusinessDateNotAllowedIssue = 300072;    
			//投票议案不存在 
            static const ATPErrorCodeType kRVotingProposalNotExist = 300073;    
			//投资者适当性信息不存在 
            static const ATPErrorCodeType kRCustQualificationInfoNotFound = 300074;    
			//专业投资者身份不符合业务要求 
            static const ATPErrorCodeType kRProfessionalKindNotMatchBiz = 300075;    
			//客户风险等级不满足业务要求 
            static const ATPErrorCodeType kRRiskLevelNotMatchBiz_1 = 300076;    
			//客户风险等级不满足业务最低要求 
            static const ATPErrorCodeType kRRiskLevelNotMatchBiz_2 = 300077;    
			//客户风险等级与业务的不匹配 
            static const ATPErrorCodeType kRRiskLevelNotMatchBiz_3 = 300078;    
			//客户投资品种集合与业务的不匹配 
            static const ATPErrorCodeType kRInvestVarietyNotMatchBiz = 300079;    
			//客户可承受亏损等级与业务的不匹配 
            static const ATPErrorCodeType kRDeficitLevelNotMatchBiz = 300080;    
			//专业投资者身份不符合标的要求 
            static const ATPErrorCodeType kRProfessionalKindNotMatchSec = 300081;    
			//客户投资期限不满足标的要求 
            static const ATPErrorCodeType kRInvestPeridNotMatchSec = 300082;    
			//客户风险等级不满足标的最低要求 
            static const ATPErrorCodeType kRRiskLevelNotMatchSec_2 = 300083;    
			//客户风险等级与标的的不匹配 
            static const ATPErrorCodeType kRRiskLevelNotMatchSec_3 = 300084;    
			//客户投资品种集合与标的的不匹配 
            static const ATPErrorCodeType kRInvestVarietyNotMatchSec = 300085;    
			//客户可承受亏损等级与标的的不匹配 
            static const ATPErrorCodeType kRDeficitLevelNotMatchSec = 300086;    
			//该股东账户不存在对应司法冻结记录 
            static const ATPErrorCodeType kRJudicialFreezeRecordNotExist = 300087;    
			//不存在对应司法冻结记录 
            static const ATPErrorCodeType kRJudicialFreezeRecordNotMatch = 300088;    
			//超过最大可冻结数量 
            static const ATPErrorCodeType kROutOffTheMaxFreezeQty = 300089;    
			//未知申报类型 
            static const ATPErrorCodeType kRUnknowDeclarationType = 300090;    
			//触发对敲风控指标规则 
            static const ATPErrorCodeType kRTriggerRulesOfContraTrade = 300091;    
			//投资者账户标的白名单不存在，无法进行开仓交易 
            static const ATPErrorCodeType kRInvestorUnderlyingSecurityNotInWhiteList = 300092;    
			//可用保证金低于设置的下限值 
            static const ATPErrorCodeType kRAvailableAmtLowerThanLimit = 300093;    
			//客户累计持仓超过限额 
            static const ATPErrorCodeType kRInvestorAllPositionLimitOut = 300094;    
			//客户多头持仓数量超过限额 
            static const ATPErrorCodeType kRInvestorAllLongPositionLimitOut = 300095;    
			//客户空头持仓数量超过限额 
            static const ATPErrorCodeType kRInvestorAllShortPositionLimitOut = 300096;    
			//客户双边持仓数量超过限额 
            static const ATPErrorCodeType kRInvestorAllBothPositionLimitOut = 300097;    
			//客户标的总持仓数量超过限额 
            static const ATPErrorCodeType kRInvestorContractAllPositionLimitOut = 300098;    
			//订单基础字段的格式检查不通过 
            static const ATPErrorCodeType kRBasicFieldsError = 300099;    
			//订单被交易所拒绝，详细的交易所撤单代码见OrdRejReason字段 
            static const ATPErrorCodeType kRExchangeReject = 300100;    
			//网关开关关闭 
            static const ATPErrorCodeType kRRejSwitchOff = 300101;    
			//无法从路由表中找到股东账户对应的TE分区号 
            static const ATPErrorCodeType kRRejAccountid = 300102;    
			//业务类型错误 
            static const ATPErrorCodeType kRRejBusinessType = 300103;    
			//市场字段错误 
            static const ATPErrorCodeType kRRejMarketID = 300104;    
			//买卖方向错误 
            static const ATPErrorCodeType kRRejSide = 300105;    
			//订单类型错误 
            static const ATPErrorCodeType kRRejOrdType = 300106;    
			//订单有效期错误 
            static const ATPErrorCodeType kRRejTimeInForce = 300107;    
			//信用标识错误 
            static const ATPErrorCodeType kRRejCashMargin = 300108;    
			//开平仓标识错误 
            static const ATPErrorCodeType kRRejPositionEffect = 300109;    
			//备兑标识错误 
            static const ATPErrorCodeType kRRejCoveredOrUncovered = 300110;    
			//转托管注册类型错误 
            static const ATPErrorCodeType kRRejDesignationTransType = 300111;    
			//证券代码错误 
            static const ATPErrorCodeType kRRejSecurityid = 300112;    
			//订单申报数量错误 
            static const ATPErrorCodeType kRRejOrderQty = 300113;    
			//价格错误 
            static const ATPErrorCodeType kRRejPrice = 300114;    
			//登录用户的IP地址未在白名单内 
            static const ATPErrorCodeType kRRejWhitList = 300115;    
			//币种错误 
            static const ATPErrorCodeType kRRejCurrency = 300116;    
			//划转方式错误 
            static const ATPErrorCodeType kRRejTransferWay = 300117;    
			//订单数量类型错误 
            static const ATPErrorCodeType kRQtyTypeError = 300118;    
			//全局流速限制 
            static const ATPErrorCodeType kRTpaRejGlobalRateLimit = 300119;    
			//终端流速限制 
            static const ATPErrorCodeType kRTpaRejIpMacRateLimit = 300120;    
			//客户号不在白名单 
            static const ATPErrorCodeType kRTpaRejCustIdNotInWhiteList = 300121;    
			//客户号在黑名单 
            static const ATPErrorCodeType kRTpaRejCustIdInBlackList = 300122;    
			//功能号权限限制 
            static const ATPErrorCodeType kRTpaRejFuncCodeNotFound = 300123;    
			//Rgw不处理委托 
            static const ATPErrorCodeType kRTpaRejMsgType = 300124;    
			//未按照三方接入会话流程进行接入 
            static const ATPErrorCodeType kRTpaRejNeedFollowTheSessionProcess = 300125;    
			//该账户已经登录 
            static const ATPErrorCodeType kRRejAlreadyLogin = 300126;    
			//该账户未登录 
            static const ATPErrorCodeType kRRejNotLoggedin = 300127;    
			//IP被禁止 
            static const ATPErrorCodeType kRRejIPAddress = 300128;    
			//恒生API错误 
            static const ATPErrorCodeType kRRejT2SDK = 300200;    
			//恒生T2SDK FundAccountId错误 
            static const ATPErrorCodeType kRRejFundAccountId = 300201;    
			//了结标识错误 
            static const ATPErrorCodeType kRRejCheckCloseOutFlag = 300202;    
			//持仓类型错误 
            static const ATPErrorCodeType kRRejLegSideErr = 300203;    
			//组合解除时，组合编码不能为空 
            static const ATPErrorCodeType kRRejSecondaryOrderIdIsErr = 300204;    
			//无法在委托路由表中找到账户对应的分区号 
            static const ATPErrorCodeType kRRejNotExistThePartition = 300205;    
			//账户无法在该分区的网关报送消息，总线管理服务中未配置网关到引擎的主题分区 
            static const ATPErrorCodeType kRRejNotExistTxEndpointPartitions = 300206;    
			//转账功能已关闭 
            static const ATPErrorCodeType kRTransferFunctionHasClosed = 300207;    
			//深圳流通类型错误 
            static const ATPErrorCodeType kRRejCheckSZCirculationType = 300208;    
			//不具备股东账户权限 
            static const ATPErrorCodeType kRCannotFindAccountID = 300209;    
			//不具备功能号权限 
            static const ATPErrorCodeType kRNoMsgTypeNoRight = 300210;    
			//构建组合时，组合策略编码不能为空 
            static const ATPErrorCodeType kRRejStrategyIdIsErr = 300211;    
			//客户号超过了流速上限 
            static const ATPErrorCodeType kRTpaRejCustIDLimit = 300212;    
			//功能号被禁止 
            static const ATPErrorCodeType kRTpaRejFuncCodeNotAllow = 300213;    
			//还款方式错误 
            static const ATPErrorCodeType kRRejRepayWay = 300214;    
			//客户号登录模式错误 
            static const ATPErrorCodeType kRCustLoginMode = 300215;    
			//客户号正在登录 
            static const ATPErrorCodeType kRCustLoggingin = 300216;    
			//客户号已经登录 
            static const ATPErrorCodeType kRCustLoggedin = 300217;    
			//客户号登录超时 
            static const ATPErrorCodeType kRCustLoginTimeout = 300218;    
			//分区号没找到 
            static const ATPErrorCodeType kRRejPartitionNo = 300219;    
			//客户端版本串校验不通过 
            static const ATPErrorCodeType kRClientVersionStringError = 300220;    
			//客户号未设置客户端版本串 
            static const ATPErrorCodeType kRCustIDIsNotExist = 300221;    
			//客户端版本串不能为空 
            static const ATPErrorCodeType kRClientVersionStringIsEmpty = 300222;    
			//风控超时 
            static const ATPErrorCodeType kRRiskControlTimout = 300223;    
			//风控拒绝 
            static const ATPErrorCodeType kRRiskControlReject = 300224;    
			//客户号和产品号流速超过限制 
            static const ATPErrorCodeType kRFlowAccessCustIDAndProductIDSpeedReject = 300225;    
			//客户号和产品号流量超过限制 
            static const ATPErrorCodeType kRFlowAccessCustIDAndProductIDSumReject = 300226;    
			//用户流速超过限制 
            static const ATPErrorCodeType kRFlowAccessUserIDSpeedReject = 300227;    
			//用户流量超过限制 
            static const ATPErrorCodeType kRFlowAccessUserIDSumReject = 300228;    
			//agw流速超过限制 
            static const ATPErrorCodeType kRFlowAccessAgwSpeedReject = 300229;    
			//agw流量超过限制 
            static const ATPErrorCodeType kRFlowAccessAgwSumReject = 300230;    
			//agw接入开关关闭 
            static const ATPErrorCodeType kRFlowAccessAgwSwitchReject = 300231;    
			//用户接入开关关闭 
            static const ATPErrorCodeType kRFlowAccessUserIDSwitchReject = 300232;    
			//客户号和产品号接入开关关闭 
            static const ATPErrorCodeType kRFlowAccessCustIDAndProductIDSwitchReject = 300233;    
			//风控无连接拒绝 
            static const ATPErrorCodeType kRRiskControlNoConnectionReject = 300234;    
			//功能号被阻断 
            static const ATPErrorCodeType kRFuncNoBlock = 300235;    
			//当前系统繁忙，请稍后重试 
            static const ATPErrorCodeType kRSystemBusy = 300236;    
			//客户密码逐笔验证失败 
            static const ATPErrorCodeType kRGwCustPasswordVerifyFailed = 300237;    
			//系统异常拒单 
            static const ATPErrorCodeType kRMalfunctionTePartitionReject = 300238;    
			//网关用户同时在线数超限,登录失败 
            static const ATPErrorCodeType kRepeateUserOnlineLimit = 300239;    
			//会话恢复失败 
            static const ATPErrorCodeType kConnectionRetentionFailed = 300240;    
			//账户一致性校验失败 
            static const ATPErrorCodeType kAccountConsistencyVerifyFailed = 300242;    
			//网关用户级功能号禁止 
            static const ATPErrorCodeType kGwUserFunctionNoCheckFailed = 300243;    
			//接入终端管控不通过 
            static const ATPErrorCodeType kAccessTerminalCheckFaild = 300244;    
			//当前网关用户不允许使用此交易账号 
            static const ATPErrorCodeType kCustIdForbidInGwUser = 300245;    
			//客户密码验证失败 
            static const ATPErrorCodeType KCustomerPasswordVerificationFailed = 300246;    
			//网关用户原密码错误 
            static const ATPErrorCodeType KGwUserOriginalPasswordError = 300247;    
			//网关用户新密码不符合强度要求 
            static const ATPErrorCodeType KGwUserNewPasswordStrengthNotRequirements = 300248;    
			//非绑定客户,禁止修改网关用户密码 
            static const ATPErrorCodeType KGwUserNotBindCustID = 300249;    
			//网关用户密码修改失败 
            static const ATPErrorCodeType KGwUserPasswordModifyFailed = 300250;    
			//日期格式检查不通过 
            static const ATPErrorCodeType kDateFormatCheckFailed = 300251;    
			//委托方式管控不通过 
            static const ATPErrorCodeType kOrderWayCheckFailed = 300252;    
			//当前子产品不支持此类接口调用 
            static const ATPErrorCodeType kNotSupportSuchInterfaceCalls = 300254;    
			//服务器连接失败 
            static const ATPErrorCodeType kServerConnectionFailure = 300255;    
			//请求超时 
            static const ATPErrorCodeType kRequestTimeout = 300256;    
			//请求头参数不正确或签名信息错误 
            static const ATPErrorCodeType kQueryRepaymentDetailRequestError = 300257;    
			//资金账户或营业部不存在 
            static const ATPErrorCodeType kQueryRepaymentDetailFundIDOrBranchIDNotExist = 300258;    
			//查询返回数量不能小于0 
            static const ATPErrorCodeType kQueryRepaymentDetailReturnNumberError = 300259;    
			//起始索引号不能小于0 
            static const ATPErrorCodeType kQueryRepaymentDetailQueryIndexError = 300260;    
			//资金账户和营业部查不到客户信息 
            static const ATPErrorCodeType kQueryRepaymentDetailCannotFoundCustinfo = 300261;    
			//AMI返回错误 
            static const ATPErrorCodeType kRAmiReturnError = 300262;    
			//请求参数非法 
            static const ATPErrorCodeType kQueryRepaymentDetailInvalidArgument = 300263;    
			//客户号与资金账户不匹配 
            static const ATPErrorCodeType kRFundAccountNotMatchedCustID = 300264;    
			//客户号不能为空 
            static const ATPErrorCodeType kRCustIDIsEmpty = 300265;    
			//委托被拒绝，请确认柜台交易模式 
            static const ATPErrorCodeType kTradeModeRefuse = 300266;    
			//资金账号不能为空 
            static const ATPErrorCodeType kRFundIDIsEmpty = 300267;    
			//外围功能号管控不通过 
            static const ATPErrorCodeType kRPeripheralFuncCodeCheckFailed = 300268;    
			//订阅标记校验失败 
            static const ATPErrorCodeType kRSubscribeMarkCheckFailed = 300269;    
			//订阅网关用户列表校验失败 
            static const ATPErrorCodeType kRSubscribeUserCheckFailed = 300270;    
			//同网关用户订阅超上限 
            static const ATPErrorCodeType kRSubscribeUserExceedUpperLimit = 300271;    
			//一户两地客户需输入证券账户查询 
            static const ATPErrorCodeType kRAccountIdIsEmpty = 300272;    
			//无效的证券账户 
            static const ATPErrorCodeType kRInvalidAccountId = 300273;    
			//资金账户查询不到客户信息 
            static const ATPErrorCodeType kRFundIDQueryAccountIDFailed = 300274;    
			//起始查询索引号不能小于0 
            static const ATPErrorCodeType kRQueryIndexLessThanzero = 300275;    
			//平台状态信息订阅失败 
            static const ATPErrorCodeType kPlatformStateQueryClose = 300276;    
			//TE网关集群实时流速超阈值 
            static const ATPErrorCodeType kClusterFlowExceedThrd = 300277;    
			//查询结果处理异常 
            static const ATPErrorCodeType kRQueryResultProcError = 300278;    
			//融合平台找不到子产品属性 
            static const ATPErrorCodeType kRFindPlatformFailed = 300279;    
			//融合平台不支持此账户 
            static const ATPErrorCodeType kRNotSupportSuchCustomer = 300280;    
			//融合平台不支持客户号登录 
            static const ATPErrorCodeType kRCustLoginIsNotAllow = 300281;    
			//融合平台不支持客户号修改密码 
            static const ATPErrorCodeType kRNotSupportCustModifyPwd = 300282;    
			//不支持修改订阅类型 
            static const ATPErrorCodeType kRNotSupportModifySubscribeType = 300283;    
			//订阅资金账户列表校验失败 
            static const ATPErrorCodeType kRSubscribeFundAccountCheckFailed = 300284;    
			//资金账户订阅超上限 
            static const ATPErrorCodeType kRSubscribeFundAccountExceedUpperLimit = 300285;    
			//匿名连接不允许调用此订阅接口 
            static const ATPErrorCodeType kRNotSupportByAnonymousUser = 300286;    
			//网关用户实时流速超阈值 
            static const ATPErrorCodeType kRUserFlowExceedThrd = 300287;    
			//非一户两地账户不支持该接口 
            static const ATPErrorCodeType kRErrorAccountType = 300288;    
			//不具备操作该投资组合的权限 
            static const ATPErrorCodeType kRNotSupportByShareUnit = 300289;    
			//投资组合不存在 
            static const ATPErrorCodeType kRAgwShareUnitNotExist = 300290;    
			//证券账户与投资组合不匹配 
            static const ATPErrorCodeType kRAgwAccountIDNotMatchShareUnit = 300291;    
			//投资组合无法获取唯一证券账户 
            static const ATPErrorCodeType kRAgwAccountIDNotMatchOnlyShareUnit = 300292;    
			//投资组合无法获取证券账户 
            static const ATPErrorCodeType kRAgwShareUnitNotGetAccountID = 300293;    
			//FPGA信息未加载 
            static const ATPErrorCodeType kRFpgaInfoNotReady = 300294;    
			//交易链路异常 
            static const ATPErrorCodeType kRTransactionLinkException = 300295;    
			//账号登出信息不完整 
            static const ATPErrorCodeType kRCustLogoutIncomplete = 300296;    
			//其他错误 
            static const ATPErrorCodeType kRRejOther = 300999;    
			//客户标的权利仓持仓数量超过限额 
            static const ATPErrorCodeType kRInvestorContractRightPositionLimitOut = 301001;    
			//客户标的当日累计买入开仓数量超过限额 
            static const ATPErrorCodeType kRInvestorContractBuyOpenLimitOut = 301002;    
			//客户标的当日累计卖出开仓数量超过限额 
            static const ATPErrorCodeType kRInvestorContractSellOpenLimitOut = 301003;    
			//客户标的当日累计开仓数量超过限额 
            static const ATPErrorCodeType kRInvestorContractOpenLimitOut = 301004;    
			//资金账户限制该业务 
            static const ATPErrorCodeType kRFundAccountRightDenied = 301005;    
			//资金账户无对应委托方式 
            static const ATPErrorCodeType kRFundAccountNoOrderWay = 301006;    
			//找不到可撤销的订单 
            static const ATPErrorCodeType kRCannotFindCancelableOrder = 301007;    
			//委托金额超出系统所能处理的范围 
            static const ATPErrorCodeType kROrderAmtRangeOut = 301008;    
			//券商无代理该ETF申赎业务资格 
            static const ATPErrorCodeType kRBrokerUnsupportedSecurityID = 301009;    
			//客户号在客户信息表中不存在 
            static const ATPErrorCodeType kRCustIDNonExist = 301010;    
			//客户密码错误 
            static const ATPErrorCodeType kRCustPWDError = 301011;    
			//客户号未启用 
            static const ATPErrorCodeType kRCustIDNotEnable = 301012;    
			//报价单未达到最小数量要求 
            static const ATPErrorCodeType kRQuoteOrderQtyFailureToMeetTheRequirement = 301013;    
			//报价单未达到最大买卖价差要求 
            static const ATPErrorCodeType kRQuoteOrderPriceFailureToMeetTheRequirement = 301014;    
			//客户投资期限不满足业务要求 
            static const ATPErrorCodeType kRInvestPeridNotMatchBiz = 301015;    
			//客户未开通交易退市整理期证券的权限 
            static const ATPErrorCodeType kRAccountSecurityStatus8NonPrivileged = 301016;    
			//客户未开通买入退市后协议转让证券的权限 
            static const ATPErrorCodeType kRAccountSecurityStatus9NonPrivileged = 301017;    
			//出库标准券使用率高于给定值 
            static const ATPErrorCodeType kRStandardSecurityUsageRateNoPassed = 301018;    
			//债券质押负债率过高 
            static const ATPErrorCodeType kRFinancingDebtRatioNoPassed = 301019;    
			//债券质押回购放大倍数过高 
            static const ATPErrorCodeType kRRepoAmplMulNoPassed = 301020;    
			//该证券不允许入库 
            static const ATPErrorCodeType kRPledgeInNotAllowed = 301021;    
			//该证券不允许出库 
            static const ATPErrorCodeType kRPledgeOutNotAllowed = 301022;    
			//该证券的折算率为零 
            static const ATPErrorCodeType kRContractMultiplieIsZero = 301023;    
			//入库集中度高于给定值 
            static const ATPErrorCodeType kRPledgeCentrationNoPassed = 301024;    
			//标准券股份不足 
            static const ATPErrorCodeType kRStandardSecurityShareInsufficient = 301025;    
			//质押券股份不足 
            static const ATPErrorCodeType kRPledgeSecurityShareInsufficient = 301026;    
			//质押出入库不允许撤单 
            static const ATPErrorCodeType kRPledgeInOrOutCancelNotAllowed = 301027;    
			//报价单买价超过价格偏离度 
            static const ATPErrorCodeType kRQuoteOrderBidPxOverPriceDeviateRate = 301028;    
			//报价单卖价超过价格偏离度 
            static const ATPErrorCodeType kRQuoteOrderOfferPxOverPriceDeviateRate = 301029;    
			//实时保证金占用超过限额 
            static const ATPErrorCodeType kRMarginOccupyOverUpperLimit = 301030;    
			//报价单买卖价格倒挂 
            static const ATPErrorCodeType kRQuoteOrderSellPriceLowerThanBuyPrice = 301031;    
			//债券质押回购融资额度高于阈值 
            static const ATPErrorCodeType kRBeyondFinancingAmountLimit = 301032;    
			//无涨跌幅限制的证券未配置溢价率 
            static const ATPErrorCodeType kRUnlimitMarketOrderNoPremiumRate = 301033;    
			//不允许无涨跌幅限制的证券交易 
            static const ATPErrorCodeType kRNotPermitUnlimitMarketOrder = 301034;    
			//未找到etf的成分股基础证券信息 
            static const ATPErrorCodeType kRNotFoundETFComponentSecurity = 301035;    
			//融资负债率过高 
            static const ATPErrorCodeType kRBondDebtRatioOverRange = 301036;    
			//重复的请求 
            static const ATPErrorCodeType kRFundTransferAgwSeqIdRepeat = 301037;    
			//资金账号存在多个分区 
            static const ATPErrorCodeType kRFundTransferFundIsMultiple = 301038;    
			//资金信息不存在 
            static const ATPErrorCodeType kRFundTransferFundIsUnKnow = 301039;    
			//不支持的货币类型 
            static const ATPErrorCodeType kRFundTransferCurrencyNotSupport = 301040;    
			//金额必须大于0 
            static const ATPErrorCodeType kRFundTransferAmountMustMoreThanZero = 301041;    
			//金额格式不正确 
            static const ATPErrorCodeType kRFundTransferAmountFormatErr = 301042;    
			//调整金额或调整后的金额不在有效金额范围 
            static const ATPErrorCodeType kRFundTransferAmountNotInValidRange = 301043;    
			//转出资金不足 
            static const ATPErrorCodeType kRFundTransferAmountInsufficient = 301044;    
			//外部系统拒绝 
            static const ATPErrorCodeType kRThirdPartyReject = 301045;    
			//发送外部系统失败 
            static const ATPErrorCodeType kRSendThirdPartyFail = 301046;    
			//发生冲正且冲正失败 
            static const ATPErrorCodeType kRFundTransferThirdPartyRollbackFail = 301047;    
			//发生冲正且冲正成功 
            static const ATPErrorCodeType kRFundTransferThirdPartyImplRollback = 301048;    
			//非港股通证券代码不允许买入 
            static const ATPErrorCodeType kRNotHKThroughSecuritySellNotAllowed = 301049;    
			//零股买卖委托数量大于一手 
            static const ATPErrorCodeType kRZeroSellTradeQtyMoreThanOneHand = 301050;    
			//零股卖出失败,委托数量不等于持仓量 
            static const ATPErrorCodeType kRZeroSellQtyNotEqualAva = 301051;    
			//市价委托的保护价在价格范围之外 
            static const ATPErrorCodeType kRTibOrderPriceRangOut = 301052;    
			//申报价格小于0.01,必须以零股类型订单申报 
            static const ATPErrorCodeType kRPriceLowMustOddType = 301053;    
			//不允许零股买入 
            static const ATPErrorCodeType kRZeroShareBuyNotAllowed = 301054;    
			//不允许零股卖出 
            static const ATPErrorCodeType kRZeroShareSellNotAllowed = 301055;    
			//不允许整手买入 
            static const ATPErrorCodeType kRWholeShareBuyNotAllowed = 301056;    
			//不允许整手卖出 
            static const ATPErrorCodeType kRWholeShareSellNotAllowed = 301057;    
			//港股通业务信息未找到 
            static const ATPErrorCodeType kRHKStocksThroughBizInfoNotFound = 301058;    
			//港股通汇率配置信息未找到 
            static const ATPErrorCodeType kRHKStocksThroughExchangeRateNotFound = 301059;    
			//非港股通交易日,不允许交易 
            static const ATPErrorCodeType kRHKStocksThroughNotTradeDay = 301060;    
			//整手卖出失败,委托数量不是数量单位整数倍 
            static const ATPErrorCodeType kRWholeSellOrderQtyStallError = 301061;    
			//零股买入失败,委托数量大于等于一手 
            static const ATPErrorCodeType kRZeroBuyTradeQtyMoreThanOneHand = 301062;    
			//港股通投票证券代码与其对应的公告编号、投票议案，在港股通信息表中未找到 
            static const ATPErrorCodeType kRHKVotingNoNonExist = 301063;    
			//投票单不在港股通投票期内 
            static const ATPErrorCodeType kRHKVotingDateNotAllowed = 301064;    
			//当前议案不容许投赞成票 
            static const ATPErrorCodeType kRHKVotingAllowNotAllowed = 301065;    
			//当前议案不容许投反对票 
            static const ATPErrorCodeType kRHKVotingOpposeNotAllowed = 301066;    
			//当前议案不容许投弃权票 
            static const ATPErrorCodeType kRHKVotingWaiverNotAllowed = 301067;    
			//非担保品标的 
            static const ATPErrorCodeType kRNotCrdCollateral = 301068;    
			//公司行为代码不存在 
            static const ATPErrorCodeType kRHKCorporateActionIDNotExist = 301069;    
			//不属于股票选择权红利 
            static const ATPErrorCodeType kRNotDividendStockOption = 301070;    
			//公司行为不在申报范围内 
            static const ATPErrorCodeType kRHKCorporateActionNotInAviliableRange = 301071;    
			//未找到对应的价格档位 
            static const ATPErrorCodeType kRHKStocksThroughPriceTickNotFound = 301072;    
			//港股投票查询的3种投票数量必须为空 
            static const ATPErrorCodeType kRHKVotingQueryFieldError = 301073;    
			//港股投票查询必须由券商发起 
            static const ATPErrorCodeType kRHKVotingQueryMustBeMember = 301074;    
			//港股权益转托管数量不足 
            static const ATPErrorCodeType kRHKEntitlementDTInsufficient = 301075;    
			//港股转托管委托不能同时选择证券编码和权益编码 
            static const ATPErrorCodeType kRHKDTIDBothInsufficient = 301076;    
			//港股通转托管权益编码未找到 
            static const ATPErrorCodeType kRHKEntitlementNumberNotFound = 301077;    
			//港股公司行为必须由会员发起 
            static const ATPErrorCodeType kRHKCorporateActionQueryMustBeMember = 301078;    
			//港股公司行为历史撤单委托不能撤单 
            static const ATPErrorCodeType kRHKCorporateActionHistoryCOrderCantNotCancel = 301079;    
			//标的清单中证券代码不存在 
            static const ATPErrorCodeType kRCrdUnderlyingNonExist = 301080;    
			//客户信息在信用资料表中不存在 
            static const ATPErrorCodeType kRCreditInfoNonExist = 301081;    
			//融资融券专户不存在 
            static const ATPErrorCodeType kRSpecialAccountNonExist = 301082;    
			//非融资标的 
            static const ATPErrorCodeType kRNotCrdBuyUnderlying = 301083;    
			//信用额度信息不存在 
            static const ATPErrorCodeType kRCreditLimitNotExist = 301084;    
			//信用额度不足 
            static const ATPErrorCodeType kRCreditLimitNotEnough = 301085;    
			//信用账户合同状态不正常 
            static const ATPErrorCodeType kRCreditInfoUnNomal = 301086;    
			//偿还数量超过最大可委托数量 
            static const ATPErrorCodeType kROutOfAvalibalQty = 301087;    
			//非信用账户 
            static const ATPErrorCodeType kRNotCreditAccount = 301088;    
			//非融资融券专户 
            static const ATPErrorCodeType kRNotSpecialAccount = 301089;    
			//信用账户业务被黑名单限制 
            static const ATPErrorCodeType kRCreditAccountForbid = 301090;    
			//客户融资融券保证金可用余额不足 
            static const ATPErrorCodeType kRMarginBalanceNotEnough = 301091;    
			//认沽期权买入保护性策略校验失败 
            static const ATPErrorCodeType kRProtectiveStrategyCheckFailed = 301092;    
			//风控限制交易权限 
            static const ATPErrorCodeType kRRiskForbidTradeRights = 301093;    
			//非融券标的 
            static const ATPErrorCodeType kRNotCrdSellUnderlying = 301094;    
			//担保率不存在 
            static const ATPErrorCodeType kRMarginRatioNotExist = 301095;    
			//重复的请求 
            static const ATPErrorCodeType kRAgwSeqIdDuplicate = 301096;    
			//偿还金额超过最大可偿还金额 
            static const ATPErrorCodeType kROutOfAvalibalAmt = 301097;    
			//该委托方式禁止此业务 
            static const ATPErrorCodeType kROrderWayNonPrivileged = 301098;    
			//策略编码不存在 
            static const ATPErrorCodeType kROptionStrategyKeyNonExist = 301100;    
			//构建的日期不在允许范围 
            static const ATPErrorCodeType kRCombinationDateOut = 301101;    
			//期权解除策略保证金组合持仓不足 
            static const ATPErrorCodeType kRRemoveCombinationPositionInsufficient = 301102;    
			//期权组合策略保证金合约持仓不足 
            static const ATPErrorCodeType kRCombinationContractPositionInsufficient = 301103;    
			//期权组合策略保证金订单中的合约腿跟交易所策略不匹配 
            static const ATPErrorCodeType kRCombinationContractRuleError = 301104;    
			//保证金账号不存在 
            static const ATPErrorCodeType kROptionMarginFundIDNonExist = 301105;    
			//成分合约单位不相等 
            static const ATPErrorCodeType kRCombinationContractUnitDiff = 301106;    
			//委托带的腿数与规则表中的腿数不相同 
            static const ATPErrorCodeType kROrderLegsDiffSecurityRule = 301107;    
			//合约标的不在此策略可使用范围内 
            static const ATPErrorCodeType kRContractUnderlyingStrategyError = 301108;    
			//合约不在此策略可使用范围内 
            static const ATPErrorCodeType kRContractStrategyError = 301109;    
			//腿组合合约标的不一致 
            static const ATPErrorCodeType kRContractDiffUnderlyingID = 301110;    
			//腿组合合约标的一致 
            static const ATPErrorCodeType kRContractSameUnderlyingID = 301111;    
			//腿组合合约到期日不完全相同 
            static const ATPErrorCodeType kRContractLastTradeDateDiff = 301112;    
			//腿组合合约到期日完全相同 
            static const ATPErrorCodeType kRContractLastTradeDateSame = 301113;    
			//备兑合约不能做组合 
            static const ATPErrorCodeType kRCoverContractNotAllow = 301114;    
			//备兑开仓存在缺口不允许开仓 
            static const ATPErrorCodeType kRCoveredOpenForbid = 301115;    
			//备兑持仓数不足 
            static const ATPErrorCodeType kRCoveredPositionLimitOut = 301116;    
			//锁定数不足 
            static const ATPErrorCodeType kRCoveredLockLimitOut = 301117;    
			//备兑不支持认沽合约 
            static const ATPErrorCodeType kRCoveredPutForbid = 301118;    
			//期权合并行权腿数量错误 
            static const ATPErrorCodeType kRCombinationLegNotEnough = 301119;    
			//期权合并行权腿认沽认购类型不能相同 
            static const ATPErrorCodeType kRCombinationLegCallOrPutConsistent = 301120;    
			//期权合并行权认沽行权价格必须高于认购行权价格 
            static const ATPErrorCodeType kRComninationPutExPriceMustUpCallExPrice = 301121;    
			//期权合并行权委托数量必须为申报数量基数的整数倍 
            static const ATPErrorCodeType kRCombinationModCardinalityNotZero = 301122;    
			//权利仓的净持仓量不足以进行合并行权 
            static const ATPErrorCodeType kRCumExerciseQtyThanNetPosition = 301123;    
			//未查询到维持担保比例设置 
            static const ATPErrorCodeType kRCanNotFindMntGuaranteeRatio = 301124;    
			//维持担保比例低于提取线 
            static const ATPErrorCodeType kRLowerOfExtractLine = 301125;    
			//无法获取客户在集中交易的配置信息 
            static const ATPErrorCodeType kRFundTransferCustCentralSysCfgError = 301126;    
			//转处置标的证券不是被处置账户应收证券 
            static const ATPErrorCodeType kRTurnToDisposalSecurityIDInvalid = 301127;    
			//转处置申报数量大于被处置账户应收证券数量 
            static const ATPErrorCodeType kRTurnToDisposalOrderQtyOutRange = 301128;    
			//担保品证券集中度超限 
            static const ATPErrorCodeType kRGageConcentrationRatioExceedLimit = 301130;    
			//余券划转累计数量超过应归还投资者余券数量 
            static const ATPErrorCodeType kROutOfExtraSecurityOut = 301131;    
			//不得转入或买入担保品折算率为零的证券 
            static const ATPErrorCodeType kRNoGageInWithZeroGageRatio = 301132;    
			//密码解密失败 
            static const ATPErrorCodeType kRPasswordDecryptFail = 301133;    
			//当前账户指定标的无余券，不需要划转 
            static const ATPErrorCodeType kRNoExtraSecurityNeedOut = 301134;    
			//两融通用交易参数不存在 
            static const ATPErrorCodeType kRNoCRDTradeParam = 301135;    
			//客户号与证券帐户不匹配 
            static const ATPErrorCodeType kRAccountIDNotMatchedCustID = 301136;    
			//转处置代码非期权标的代码 
            static const ATPErrorCodeType kRTurnToDisposalUnderlyingSecurityIDInvalid = 301137;    
			//委托数量或总额度小于最低限制 
            static const ATPErrorCodeType kRQtyAndFundLowLimited = 301138;    
			//报盘交易单元无法在该分区的引擎报单，总线管理服务中未配置引擎到报盘的主题分区 
            static const ATPErrorCodeType kRNotExistThePartition = 301139;    
			//大宗交易定价成交申报无法撤单 
            static const ATPErrorCodeType kRTransactionCancelOrderNotAllowed = 301140;    
			//现货标的当日累计持仓数量超过限额 
            static const ATPErrorCodeType kRCashUnderlyingPosLimitOut = 301141;    
			//现货标的价格超过价格偏离度 
            static const ATPErrorCodeType kRCashOrderPxOverPriceDeviateRate = 301142;    
			//转处置返还数量大于剩余转处置数量 
            static const ATPErrorCodeType kRTurnToDisposalReturnOrderQtyOutRange = 301143;    
			//客户标的义务仓持仓数量超过限额 
            static const ATPErrorCodeType kRInvestorContractDutyPositionLimitOut = 301144;    
			//超过可减持的额度 
            static const ATPErrorCodeType kRReduceShareOverRange = 301145;    
			//合同状态异常禁止此业务 
            static const ATPErrorCodeType kRContractStatusNonPrivileged = 301146;    
			//组合策略构成头寸方向不符合要求 
            static const ATPErrorCodeType kRLegSideError = 301148;    
			//组合策略构成合约行权价格不符合要求 
            static const ATPErrorCodeType kRExercisrPriceError = 301149;    
			//组合策略构成合约到期日不符合要求 
            static const ATPErrorCodeType kRExercisrDateError = 301150;    
			//组合策略构成合约认沽认购类型不符合要求 
            static const ATPErrorCodeType kRLegCallOrPutError = 301151;    
			//组合策略构成合约备兑标签不符合要求 
            static const ATPErrorCodeType kRLegCoveredOrUncoveredError = 301152;    
			//组合策略合约为非标合约 
            static const ATPErrorCodeType kRCombinationStrategyIsNotStandard = 301153;    
			//累计减持额度超过限制 
            static const ATPErrorCodeType kRCumReduceShareOverRange = 301154;    
			//信用与普通账户对应关系不存在 
            static const ATPErrorCodeType kRCreNormalAccMapNotFound = 301155;    
			//组合策略成分合约顺序不符合规则 
            static const ATPErrorCodeType kRLegSideOrderError = 301156;    
			//不存在股份负债 
            static const ATPErrorCodeType kRNotShareLiability = 301157;    
			//两融专户禁止该交易业务 
            static const ATPErrorCodeType kRSpecialAccountNotAllowTheBusiness = 301158;    
			//融资专户禁止该交易业务 
            static const ATPErrorCodeType kRFinanSpecialAccountNotAllowTheBusiness = 301159;    
			//组合持仓不存在或组合持仓与合约账户不匹配 
            static const ATPErrorCodeType kRRemoveCombinationPositionNotExitOrNotRight = 301160;    
			//该组合持仓不存在 
            static const ATPErrorCodeType kRRemoveCombinationPositionNotExit = 301161;    
			//普通义务仓数量不足 
            static const ATPErrorCodeType kRDutyPositionLimitOut = 301162;    
			//组合策略编码不存在 
            static const ATPErrorCodeType kRCombinationIDError = 301163;    
			//融资保底额度信息不存在 
            static const ATPErrorCodeType kRNotFoundBuyPrivateAmount = 301164;    
			//融券保底额度信息不存在 
            static const ATPErrorCodeType kRNotFoundSellPrivateAmount = 301165;    
			//融资专户资金余额不足 
            static const ATPErrorCodeType kRMarginSpecialFundNotEnough = 301166;    
			//融券专户股份余额不足 
            static const ATPErrorCodeType kRMarginSpecialShareNotEnough = 301167;    
			//认沽合约不能做备兑开仓 
            static const ATPErrorCodeType kRLegPutNotAllowCoverOpen = 301168;    
			//卖出股份不含受限股 
            static const ATPErrorCodeType kRNotHaveRestrictShare = 301169;    
			//大宗减持业务开关被关闭 
            static const ATPErrorCodeType kRReduceHoldingQuoteSwithchOff = 301170;    
			//融券保底额度额外信息不存在 
            static const ATPErrorCodeType kRNotExtraFoundSellPrivateAmount = 301171;    
			//未获取到汇率信息 
            static const ATPErrorCodeType kRCustFeeConfigError = 301173;    
			//资金账号不支持该币种 
            static const ATPErrorCodeType kRFundNotSupportTheCurrency = 301174;    
			//垫券还券业务垫券申报数量超过投资人行权净应付数量 
            static const ATPErrorCodeType kRCouponSwapOrderQtyOutRange = 301175;    
			//垫券还券业务还券申报数量超过投资人现货证券账户可用数量 
            static const ATPErrorCodeType kRCouponSwapOrderQtyOutRangeCashAvailable = 301176;    
			//投资人没有该证券行权净应付证券 
            static const ATPErrorCodeType kRCouponSwapSecurityIDInvalid = 301177;    
			//待清偿申报数量大于行权标的证券净应收数量 
            static const ATPErrorCodeType kRDeductionOrderQtyOutRange = 301178;    
			//待清偿申报账户类型错误 
            static const ATPErrorCodeType kRDeductionAccountTypeError = 301179;    
			//该投资人没有该证券行权净应收证券 
            static const ATPErrorCodeType kRDeductionAccountNoPaidPos = 301180;    
			//平仓状态禁止此业务 
            static const ATPErrorCodeType kRCloseStatusNonPrivilegedCreditTrade = 301181;    
			//可用数量小于最低卖出数量必须全部卖出 
            static const ATPErrorCodeType kRLessThanMinSellQtyMustSellAll = 301182;    
			//卖出零股数量与可用数量的零股数量不相同 
            static const ATPErrorCodeType kRSellZeroNotEqualAvailableZero = 301183;    
			//撤单次数超限 
            static const ATPErrorCodeType kRCancelTimeOverlimit = 301184;    
			//客户费用信息不存在 
            static const ATPErrorCodeType kRFundFeeInfoFail = 301185;    
			//证券类型对应的证券品种不存在 
            static const ATPErrorCodeType kRGetSecurityVarietyFail = 301186;    
			//卖出所得资金小于费用，不允许交易 
            static const ATPErrorCodeType kRSellFundLessThanFeeProhibitd = 301187;    
			//卖出资金小于费用，请谨慎操作（仅提示作用，委托正常处理） 
            static const ATPErrorCodeType kRSellFundLessThanFeeAlarm = 301188;    
			//转处置账户或交易单元校验失败 
            static const ATPErrorCodeType kRTurnToDisposalAccountInvalid = 301189;    
			//信用账户禁止该交易业务 
            static const ATPErrorCodeType kRCreditAccountNotAllowTheBusiness = 301190;    
			//上海不支持备兑转普通 
            static const ATPErrorCodeType kRShanghaiTransUncoveredUnsupported = 301195;    
			//备兑开仓只支持卖方向 
            static const ATPErrorCodeType kRCoverOpenOnlySupportSell = 301196;    
			//备兑平仓只支持买方向 
            static const ATPErrorCodeType kRCoverCloseOnlySupportBuy = 301197;    
			//超过系统最大可拆单数 
            static const ATPErrorCodeType kRThanSplitUpLimit = 301198;    
			//拆单数量参数错误 
            static const ATPErrorCodeType kRSplitOrderQtyError = 301199;    
			//批量类型错误 
            static const ATPErrorCodeType kRBatchTypeError = 301200;    
			//融资融券未知三方系统类型 
            static const ATPErrorCodeType kRUnknownThirdSystemType = 301201;    
			//客户未到期融资负债尚未更新 
            static const ATPErrorCodeType kRRepoUnexpiredInfoNotUpdate = 301202;    
			//券商发起委托, 投资人不能撤单 
            static const ATPErrorCodeType kRUnableToCancellMemberOrder = 301203;    
			//客户交易密码加密失败 
            static const ATPErrorCodeType kREncryptFail = 301204;    
			//客户交易密码解密失败 
            static const ATPErrorCodeType kRDecryptFail = 301205;    
			//客户交易密码强度校验失败 
            static const ATPErrorCodeType kRStrengthCheckFail = 301206;    
			//未收到收盘价且证券价格无涨跌幅，无法冻结资金 
            static const ATPErrorCodeType kRNoClosePriceAndNoPriceLimit = 301208;    
			//小于定向可转债协议转让数量门槛（张） 
            static const ATPErrorCodeType kRLessDirectionalConvertibleBondsMin = 301209;    
			//当前不支持的直接还券类型 
            static const ATPErrorCodeType kRRedeliverTypeDeny = 301210;    
			//客户未开通买入退市整理期首日证券的权限 
            static const ATPErrorCodeType kRAccountSecurityStatus12NonPrivileged = 301211;    
			//融券卖出规模超过限额 
            static const ATPErrorCodeType kRPeakOverRiskExposure = 301212;    
			//交易节点不存在 
            static const ATPErrorCodeType kRRootNumNotExist = 301213;    
			//组合单边平仓合约不是组合持仓中的义务仓 
            static const ATPErrorCodeType kRCombinationBuyCloseContractInvaild = 301230;    
			//组合持仓数量不足 
            static const ATPErrorCodeType kRCombinationPositionInsufficient = 301231;    
			//消息业务被禁用 
            static const ATPErrorCodeType kRMsgBusinessForbid = 301232;    
			//不满足特殊维保比例检查项 
            static const ATPErrorCodeType kRLowerThanSpecExtractLine = 301233;    
			//营业部为空 
            static const ATPErrorCodeType kRBranchIDIsNull = 301234;    
			//成交申报类型错误 
            static const ATPErrorCodeType kRTradeReportTypeError = 301235;    
			//订单已撤销 
            static const ATPErrorCodeType kROrderCanceled = 301236;    
			//没有对应的转发信息 
            static const ATPErrorCodeType kRNoFindForwardInfo = 301237;    
			//该笔订单已经处理 
            static const ATPErrorCodeType kROrderProcessed = 301238;    
			//债券协商成交对手方不支持撤单 
            static const ATPErrorCodeType kRBondConsultTradeCounterpartyNoCanceled = 301239;    
			//没有找到对应本方交易信息表信息 
            static const ATPErrorCodeType kRNoFindOurPartyTradeInfo = 301240;    
			//适当性业务检查参数配置不存在 
            static const ATPErrorCodeType kRBusinessTypeToCodeNotExist = 301241;    
			//交易主体类型错误 
            static const ATPErrorCodeType kRInvestorTypeError = 301242;    
			//账户无减持额度信息 
            static const ATPErrorCodeType kRAccountNoReductionQuoteInfo = 301243;    
			//已配置多种还券类型，请选择还券类型后再次提交委托 
            static const ATPErrorCodeType kRMultiRedeliverType = 301244;    
			//外部系统股份划拨失败 
            static const ATPErrorCodeType kRExtShareTransferFail = 301245;    
			//无效指定偿还合约 
            static const ATPErrorCodeType kRDesignateContractInvalid = 301246;    
			//当前还券类型不支持偿还指定合约 
            static const ATPErrorCodeType kRDesignateContractRedeliverTypeDeny = 301247;    
			//划拨后的数量超过上下限 
            static const ATPErrorCodeType kRTransferResultQtyOverRange = 301248;    
			//新债平台不支持新指定账户交易 
            static const ATPErrorCodeType kRNewBondNoAllowOrder = 301249;    
			//该业务类别或证券类别不在夜市委托业务白名单中 
            static const ATPErrorCodeType kRNotAllowedInNightOrderWhiteList = 301250;    
			//夜市委托不允许市价委托 
            static const ATPErrorCodeType kRNightStatusNotAllowMarketOrder = 301251;    
			//夜市委托不允许减持受限股 
            static const ATPErrorCodeType kRNightStatusNotAllowReduceLimitedShare = 301252;    
			//上海市场不支持组合单边平仓 
            static const ATPErrorCodeType kRShanghaiCombinationOneLegClosedUnsupported = 301253;    
			//指定合约偿还未配置指定合约偿还参数 
            static const ATPErrorCodeType kRDesignateContractRepayControlNotSet = 301254;    
			//银证转账流水查询失败 
            static const ATPErrorCodeType kRBankSecuritiesTransferStreamQueryFailed = 301255;    
			//银证转账成功,资金划入失败 
            static const ATPErrorCodeType kRBankSecuritiesTransferSuccessAndFundTransferInFailed = 301256;    
			//资金划出成功,银证转账失败 
            static const ATPErrorCodeType kRFundTransferOutSuccessAndBankSecuritiesTransferFailed = 301257;    
			//银证转入失败 
            static const ATPErrorCodeType kRBankSecuritiesTransferOutFailed = 301258;    
			//空头证券集中度超限 
            static const ATPErrorCodeType kRNetShortGageConcentrationRatioExceedLimit = 301259;    
			//多头证券集中度超限 
            static const ATPErrorCodeType kRNetLongGageConcentrationRatioExceedLimit = 301260;    
			//融资可用额度不足 
            static const ATPErrorCodeType kRRzAvailableQuotaNotEnough = 301261;    
			//公司级风控担保品集中度触警 
            static const ATPErrorCodeType kRCorpRiskGageConcentrationRateLimit = 301262;    
			//市价委托的保护价在价格范围之外 
            static const ATPErrorCodeType kMarketPriceRangeOut = 301263;    
			//资金帐户信息不存在 
            static const ATPErrorCodeType kFundAccountIDNotExist = 301264;    
			//公司风控担保品规模预警超限 
            static const ATPErrorCodeType kCorpRiskGageConcentrationRateOverLimit = 301265;    
			//维持担保比例低于开仓线 
            static const ATPErrorCodeType kLowerThanOpenLine = 301266;    
			//系统内部报盘通道异常 
            static const ATPErrorCodeType kROrderTeOrsPartitionAbnormal = 301267;    
			//标的证券类别或风险等级不存在 
            static const ATPErrorCodeType kUnderlyingTypeorRiskLevelNotExist = 301268;    
			//撤单触发额度透支风险 
            static const ATPErrorCodeType kRqAvailableQuotaNotEnough = 301269;    
			//委托方式编码不可使用\'<,&字符 
            static const ATPErrorCodeType kROrderWayCharacterUnavailable = 301272;    
			//该资金账户不允许市价委托 
            static const ATPErrorCodeType kFundAccountIDNotAllowMarketPriceOrder = 301277;    
			//客户未签署电子合同或风险揭示书 
            static const ATPErrorCodeType kCustUnsignedEleContractOrRiskDisclosure = 301283;    
			//FPGA更新资金失败 
            static const ATPErrorCodeType kFpgaUpdateFundFailed = 301378;    
			//风控拒单，按单一账户计算超申报速率 
            static const ATPErrorCodeType kSingleAccountOrderRateExceed = 301385;    
			//风控拒单，按单一账户计算超当日申报笔数 
            static const ATPErrorCodeType kSingleAccountOrderNumberExceed = 301386;    
			//风控拒单，按账户汇总计算超申报速率 
            static const ATPErrorCodeType kAccountSummarizeOrderRateExceed = 301387;    
			//风控拒单，按账户汇总计算超当日申报笔数 
            static const ATPErrorCodeType kAccountSummarizeOrderNumberExceed = 301388;    
			//联动市场证券账户不存在 
            static const ATPErrorCodeType kRRelatedAccountNotExists = 301390;    
			//联动市场证券账户信息不完整 
            static const ATPErrorCodeType kRRelatedAccountMissed = 301391;    
			//联动市场证券账户存在多个 
            static const ATPErrorCodeType kRRelatedAccountIsMultiple = 301392;    
			//联动市场证券账户状态异常 
            static const ATPErrorCodeType kRRelatedAccountStatusAbnormal = 301393;    
			//发送联动市场消息失败，请检查网络或组件是否正常 
            static const ATPErrorCodeType kRSendRelatedMessagesFailed = 301394;    
			//该ETF的申赎类型为全实物申赎，当前API版本不支持 
            static const ATPErrorCodeType kRNotSupportCMTETFAllPhysicalPR = 301395;    
			//划拨数量非法 
            static const ATPErrorCodeType kRTransferQtyInvalid = 301396;    
			//投资组合信息表中不存在投资组合 
            static const ATPErrorCodeType kRShareUnitNotExist = 301400;    
			//证券账户与投资组合不匹配 
            static const ATPErrorCodeType kRAccountIDNotMatchShareUnit = 301401;    
			//组合无法获取唯一证券账户 
            static const ATPErrorCodeType kRAccountIDNotMatchOnlyShareUnit = 301402;    
			//自定义委托批号重复 
            static const ATPErrorCodeType kRBatchClOrdNODuplicate = 301403;    
			//用户消息序号重复 
            static const ATPErrorCodeType kRDuplicateClientSeqID = 302103;    
			//业务错误 
            static const ATPErrorCodeType kRBizError = 390001;    
			//市场休市 
            static const ATPErrorCodeType kRMarketClosed = 390002;    
			//非法消息 
            static const ATPErrorCodeType kRFieldError = 390003;    
			//对接的交易所报盘通道异常 
            static const ATPErrorCodeType kRConnectionError = 390004;    
			//报盘通道不存在 
            static const ATPErrorCodeType kROfferwayError = 390005;    
			//报盘开关已关闭 
            static const ATPErrorCodeType kRSwitchReject = 390006;    
			//指定报盘通道ID错误 
            static const ATPErrorCodeType kTheAssignOPUIDError = 390010;    
			//业务错误 
            static const ATPErrorCodeType kRCosBizError = 391001;    
			//报盘通道不存在 
            static const ATPErrorCodeType kRCosOfferwayError = 391002;    
			//报盘开关已关闭 
            static const ATPErrorCodeType kRCosSwitchReject = 391003;    
        };
        /**
         * @brief 用户ID，填入客户号或者资金账户 
         **/ 
        typedef char ATPUserIDType[17];
        /**
         * @brief 客户号ID 
         **/ 
        typedef char ATPCustIDType[17];
        /**
         * @brief 资金账户ID 
         **/ 
        typedef char ATPFundAccountIDType[17];
        /**
         * @brief 证券账户ID 
         **/ 
        typedef char ATPAccountIDType[13];
        /**
         * @brief 金额类型 N15(4) 
         **/ 
        typedef double ATPAmtType;
        /**
         * @brief 业务类型 
         **/ 
        typedef uint8_t ATPBusinessTypeType;
        struct QUANT_API ATPBusinessTypeConst
        {
			//默认值 
            static const ATPBusinessTypeType kDefault = 0xFF;    
			//所有类型 
            static const ATPBusinessTypeType kAll = 0;    
			//现货集中交易 
            static const ATPBusinessTypeType kCashAuction = 1;    
			//质押式回购交易 
            static const ATPBusinessTypeType kRepoAuction = 2;    
			//债券分销 
            static const ATPBusinessTypeType kBondDistribution = 3;    
			//期权集中交易 
            static const ATPBusinessTypeType kOptionAuction = 4;    
			//协议交易(大宗交易) 
            static const ATPBusinessTypeType kBlockTrade = 5;    
			//盘后定价交易 
            static const ATPBusinessTypeType kAfterHoursPricing = 6;    
			//转融通 
            static const ATPBusinessTypeType kRefinancing = 7;    
			//资产管理计划份额转让 
            static const ATPBusinessTypeType kAssetTransfer = 8;    
			//股票质押式回购 
            static const ATPBusinessTypeType kCashPledgeRepo = 9;    
			//约定购回 
            static const ATPBusinessTypeType kAgreedRepo = 10;    
			//质押式报价回购 
            static const ATPBusinessTypeType kPledgeQuoteRepo = 11;    
			//ETF实时申购赎回 
            static const ATPBusinessTypeType kETFRealTimePurchaseRedemption = 12;    
			//网上发行认购 
            static const ATPBusinessTypeType kIssue = 13;    
			//配售 
            static const ATPBusinessTypeType kRightsIssue = 14;    
			//债券转股回售 
            static const ATPBusinessTypeType kSwapPutback = 15;    
			//期权行权 
            static const ATPBusinessTypeType kOptionExercise = 16;    
			//开放式基金申赎 
            static const ATPBusinessTypeType kLOF = 17;    
			//要约收购 
            static const ATPBusinessTypeType kTenderOffer = 18;    
			//质押出入库业务 
            static const ATPBusinessTypeType kPledgeInOrOut = 19;    
			//质押式回购质押解押 
            static const ATPBusinessTypeType kPledgeRepoPledgeOrSign = 19;    
			//转托管 
            static const ATPBusinessTypeType kDesignation = 20;    
			//网络投票 
            static const ATPBusinessTypeType kVoting = 21;    
			//指定交易 
            static const ATPBusinessTypeType kDesignatedTransaction = 22;    
			//权证行权 
            static const ATPBusinessTypeType kWarrantsExercise = 23;    
			//密码服务 
            static const ATPBusinessTypeType kPasswordService = 24;    
			//保证金查询 
            static const ATPBusinessTypeType kOpenMarginQuery = 25;    
			//开放式基金转托管、分红方式设置 
            static const ATPBusinessTypeType kOpenFundSetting = 26;    
			//转处置 
            static const ATPBusinessTypeType kTurnToDisposal = 27;    
			//垫券还券 
            static const ATPBusinessTypeType kCouponSwap = 28;    
			//待清偿扣划 
            static const ATPBusinessTypeType kPayOff = 29;    
			//债券质押式协议回购 
            static const ATPBusinessTypeType kBondPledgeAgreementRepo = 30;    
			//分级基金实时分拆合并 
            static const ATPBusinessTypeType kStructuredFund = 31;    
			//备兑锁定解锁 
            static const ATPBusinessTypeType kCoverLockOrUnlock = 32;    
			//合并行权 
            static const ATPBusinessTypeType kOptionCombinationExercise = 33;    
			//组合保证金 
            static const ATPBusinessTypeType kOptionCombination = 34;    
			//高管额度划转 
            static const ATPBusinessTypeType kReduceHoldingTransfer = 36;    
			//期权普通与备兑互转 
            static const ATPBusinessTypeType kOptionCoveredTransUncovered = 35;    
			//港股通 
            static const ATPBusinessTypeType kHKStocksThrough = 63;    
			//港股通投票 
            static const ATPBusinessTypeType kHKVoting = 64;    
			//港股通转托管 
            static const ATPBusinessTypeType kHKDesignationTransfer = 61;    
			//港股通公司行为 
            static const ATPBusinessTypeType kHKCorporateAction = 62;    
			//盘后定价(科创板、创业板)交易业务 
            static const ATPBusinessTypeType kTibAfterHour = 97;    
			//融资融券交易业务 
            static const ATPBusinessTypeType kMarginTrade = 99;    
			//融资融券非交易业务 
            static const ATPBusinessTypeType kMarginNoTrade = 100;    
			//融资融券交易偿还 
            static const ATPBusinessTypeType kMarginRepay = 101;    
			//融资融券直接还款 
            static const ATPBusinessTypeType kMarginDirectRepay = 102;    
			//新现货集中交易 
            static const ATPBusinessTypeType kNewCashAuction = 200;    
        };
        /**
         * @brief 营业部ID 
         **/ 
        typedef char ATPBranchIDType[11];
        /**
         * @brief 请求序号；需要全局保持唯一，由API使用者负责维护 
         **/ 
        typedef int64_t ATPRequestIDType;
        /**
         * @brief 客户订单编号  
         **/ 
        typedef int64_t ATPClOrdNOType;
        /**
         * @brief 客户自定义委托批号  
         **/ 
        typedef uint64_t ATPCustomOrderNOType;
        /**
         * @brief 申报合同号 
         **/ 
        typedef char ATPClOrdIDType[11];
        /**
         * @brief 货币种类（仅支持人民币） 
         **/ 
        typedef char ATPCurrencyType[5];
        struct QUANT_API ATPCurrencyConst
        {
			//人民币 
			static constexpr ATPCurrencyType kCNY = "CNY"; 
			//美元 
			static constexpr ATPCurrencyType kUSD = "USD"; 
			//港币 
			static constexpr ATPCurrencyType kHKD = "HKD"; 
        };
        /**
         * @brief 日期,格式(YYYYmmDD) 
         **/ 
        typedef int64_t ATPDate8Type;
        /**
         * @brief ETF 成交回报类型 
         **/ 
        typedef uint8_t ATPETFTradeReportTypeType;
        /**
         * @brief 订单数量类型 
         **/ 
        typedef char ATPLotTypeType;
        /**
         * @brief 账户登录模式 
         **/ 
        typedef uint8_t ATPLoginModeType;
        struct QUANT_API ATPLoginModeConst
        {
			//客户号登录模式 
            static const ATPLoginModeType kCustIDMode = 1;    
			//资金账号登录模式 
            static const ATPLoginModeType kFundAccountIDMode = 2;    
			//自营登录模式 
            static const ATPLoginModeType kCMNullMode = 3;    
        };
        /**
         * @brief 重传模式 
         **/ 
        typedef uint8_t ATPRetransmitModeType;
        struct QUANT_API ATPRetransmitModeConst
        {
			//Quick模式 
            static const ATPRetransmitModeType kQuickMode = 0;    
			//Restart模式 
            static const ATPRetransmitModeType kRestartMode = 1;    
        };
        /**
         * @brief 市场代码 
         **/ 
        typedef uint16_t ATPMarketIDType;
        struct QUANT_API ATPMarketIDConst
        {
			//空 
            static const ATPMarketIDType kNULL = 0;    
			//上海 
            static const ATPMarketIDType kShangHai = 101;    
			//深圳 
            static const ATPMarketIDType kShenZhen = 102;    
			//香港 
            static const ATPMarketIDType kXiangGang = 103;    
			//北京 
            static const ATPMarketIDType kBeiJing = 104;    
        };
        /**
         * @brief 订单类型 
         **/ 
        typedef char ATPOrdTypeType;
        struct QUANT_API ATPOrderTypeConst
        {
			//默认值 
            static const ATPOrdTypeType kDefault = ' ';    
			//市价剩余转限价,适用于上交所（已弃用） 
            static const ATPOrdTypeType kTransferFixed = 'k';    
			//限价委托、增强限价 
            static const ATPOrdTypeType kFixedNew = 'a';    
			//本方最优 
            static const ATPOrdTypeType kLocalOptimalNew = 'b';    
			//对手方最优剩余转限价 
            static const ATPOrdTypeType kCountPartyOptimalTransferFixed = 'c';    
			//立即成交剩余撤销 
            static const ATPOrdTypeType kImmediateDealTransferCancel = 'd';    
			//全额成交或撤销 
            static const ATPOrdTypeType kFullDealOrCancel = 'e';    
			//最优五档全额成交剩余撤销 
            static const ATPOrdTypeType kOptimalFiveLevelFullDealTransferCancel = 'f';    
			//最优五档即时成交剩余转限价 
            static const ATPOrdTypeType kOptimalFiveLevelImmediateDealTransferFixed = 'g';    
			//市价剩余转限价（已弃用） 
            static const ATPOrdTypeType kMarketTransferFixed = 'h';    
			//限价全额成交或撤销（已弃用） 
            static const ATPOrdTypeType kFixedFullDealOrCancel = 'i';    
			//竞价限价（深圳）（用于港股通业务）（已弃用） 
            static const ATPOrdTypeType kSzBiddingFixed = 'j';    
			//竞价限价（上海)（用于港股通业务）（已弃用） 
            static const ATPOrdTypeType kShBiddingFixed = 'k';    
			//市价（已弃用） 
            static const ATPOrdTypeType kMarket = '1';    
			//限价（已弃用） 
            static const ATPOrdTypeType kFixed = '2';    
			//本方最优（已弃用） 
            static const ATPOrdTypeType kLocalOptimal = 'U';    
        };
        /**
         * @brief 委托方式 
         **/ 
        typedef char ATPOrderWayType;
        /**
         * @brief 订单状态 
         **/ 
        typedef uint8_t ATPOrdStatusType;
        struct QUANT_API ATPOrdStatusConst
        {
			//默认值 
            static const ATPOrdStatusType kDefault = 0xFF;    
			//已申报 
            static const ATPOrdStatusType kNew = 0;    
			//部分成交 
            static const ATPOrdStatusType kPartiallyFilled = 1;    
			//全部成交 
            static const ATPOrdStatusType kFilled = 2;    
			//部分成交部分撤销 
            static const ATPOrdStatusType kPartiallyFilledPartiallyCancelled = 3;    
			//已撤销 
            static const ATPOrdStatusType kCancelled = 4;    
			//部分撤销 
            static const ATPOrdStatusType kPartiallyCancelled = 5;    
			//已拒绝 
            static const ATPOrdStatusType kReject = 8;    
			//未申报 
            static const ATPOrdStatusType kUnSend = 9;    
			//正报 
            static const ATPOrdStatusType kSended = 10;    
			//待撤销 
            static const ATPOrdStatusType kWaitCancelled = 11;    
			//部成待撤 
            static const ATPOrdStatusType kPartiallyFilledWaitCancelled = 12;    
			//内部拒单 
            static const ATPOrdStatusType kInternalRejectOrder = 90;    
			//内部撤单 
            static const ATPOrdStatusType kInternalCanceledOrder = 91;    
			//撤单成功 
            static const ATPOrdStatusType kCanceledSuccess = 92;    
        };
        /**
         * @brief 订单查询条件 
         **/ 
        typedef uint8_t ATPOrderQueryConditionType;
        struct QUANT_API ATPOrderQueryConditionConst
        {
			//查询全部（不含撤单委托） 
            static const ATPOrderQueryConditionType kAll = 0;    
			//查询可撤单订单 
            static const ATPOrderQueryConditionType kCancelAvailable = 1;    
			//查询有成交或已经撤单订单 
            static const ATPOrderQueryConditionType kTradeExecOrCanceled = 2;    
			//查询不可撤单订单 
            static const ATPOrderQueryConditionType kUnavailableCancel = 3;    
			//查询全部委托（含撤单委托） 
            static const ATPOrderQueryConditionType kAllWithCancleOrd = 4;    
			//查询全部委托（不含TE内部拒单）（ATP3.2.3开始支持） 
            static const ATPOrderQueryConditionType kAllExceptInRejOrd = 5;    
        };
        /**
         * @brief 成交查询条件 
         **/ 
        typedef uint8_t ATPTradeOrderQueryConditionType;
        struct QUANT_API ATPTradeOrderQueryConditionConst
        {
			//按全局参数设置的模式进行查询 
            static const ATPTradeOrderQueryConditionType kGlobalParams = 0;    
			//仅查询成交回报 
            static const ATPTradeOrderQueryConditionType kTradeReport = 1;    
			//查询成交回报、撤单成功消息 
            static const ATPTradeOrderQueryConditionType kTradeReportAndCancleSuccess = 2;    
			//查询成交回报、撤单成功消息、委托拒单消息、撤单拒单消息 
            static const ATPTradeOrderQueryConditionType kAllAvailable = 3;    
        };
        /**
         * @brief 分区号 
         **/ 
        typedef uint8_t ATPPartitionType;
        /**
         * @brief 密码 
         **/ 
        typedef char ATPPasswordType[1001];
        /**
         * @brief 委托价格 N13(4) 
         **/ 
        typedef double ATPPriceType;
        /**
         * @brief 数量 N15(2) 
         **/ 
        typedef double ATPQtyType;
        /**
         * @brief 返回顺序  
         **/ 
        typedef uint8_t ATPReturnSeqType;
        struct QUANT_API ATPReturnSeqConst
        {
			//默认值 
            static const ATPReturnSeqType kDefault = 0xFF;    
			//时间正序 
            static const ATPReturnSeqType kTimeOrder = 1;    
			//时间倒序 
            static const ATPReturnSeqType kTimeOrderRe = 2;    
        };
        /**
         * @brief 证券代码 
         **/ 
        typedef char ATPSecurityIDType[9];
        /**
         * @brief 证券简称 
         **/ 
        typedef char ATPSecuritySymbolType[41];
        /**
         * @brief 买卖方向 
         **/ 
        typedef char ATPSideType;
        struct QUANT_API ATPSideConst
        {
			//默认值 
            static const ATPSideType kDefault = ' ';    
			//所有 
            static const ATPSideType kAll = '0';    
			//买 
            static const ATPSideType kBuy = '1';    
			//卖 
            static const ATPSideType kSell = '2';    
			//正回购 
            static const ATPSideType kRepo = '3';    
			//逆回购 
            static const ATPSideType kAntiRepo = '4';    
			//转股 
            static const ATPSideType kSwap = '5';    
			//回售 
            static const ATPSideType kPutBack = '6';    
			//预受要约 
            static const ATPSideType kTenderOffer = '7';    
			//解除预受要约 
            static const ATPSideType kDisTenderOffer = '8';    
			//回售撤销 
            static const ATPSideType kPutBackRevoke = '9';    
			//密码激活 
            static const ATPSideType kActivation = 'A';    
			//密码注销 
            static const ATPSideType kCancellation = 'C';    
			//场内申购 
            static const ATPSideType kPurchase = 'D';    
			//场内赎回 
            static const ATPSideType kRedeem = 'E';    
			//出借 
            static const ATPSideType kLoan = 'F';    
			//借入 
            static const ATPSideType kBorrow = 'G';    
			//场外申购 
            static const ATPSideType kPhysicalPurchase = 'P';    
			//场外赎回 
            static const ATPSideType kPhysicalRedemption = 'R';    
			//分拆 
            static const ATPSideType kOpen = 'O';    
			//合并 
            static const ATPSideType kMerge = 'M';    
			//查询 
            static const ATPSideType kQuery = 'Q';    
			//转托管 
            static const ATPSideType kDesignationTransfer = 'J';    
			//反向托管 
            static const ATPSideType kReverDsignationTransfer = 'K';    
			//锁定 
            static const ATPSideType kLock = 'Z';    
			//解锁 
            static const ATPSideType kUnLock = 'Y';    
			//转处置 
            static const ATPSideType kDisposal = '@';    
			//转处置返还 
            static const ATPSideType kDisposalReturn = '%';    
			//质押入库 
            static const ATPSideType kPledgeIn = 'H';    
			//质押出库 
            static const ATPSideType kPledgeOut = 'I';    
			//卖券还款 
            static const ATPSideType kSellRepay = 'N';    
			//买券还券 
            static const ATPSideType kBuyRedeliver = 'U';    
			//融资买入 
            static const ATPSideType kFinancingBuy = 'B';    
			//融券卖出 
            static const ATPSideType kLoanSell = 'S';    
			//直接还券 
            static const ATPSideType kReDeliver = 'T';    
			//担保品提交 
            static const ATPSideType kGageIn = 'V';    
			//担保品返还 
            static const ATPSideType kGageOut = 'L';    
			//融资平仓 
            static const ATPSideType kMarginOffset = 'W';    
			//融券平仓 
            static const ATPSideType kShortSaleOffset = 'X';    
			//券源提交 
            static const ATPSideType kSourceSecurityIn = '$';    
			//券源返还 
            static const ATPSideType kSourceSecurityOut = '*';    
			//余券划转 
            static const ATPSideType kExtraSecurityOut = '#';    
        };
        /**
         * @brief 指定查询返回数量 
         **/ 
        typedef int64_t ATPTotalNumType;
        /**
         * @brief 委托时间20190101121212001  (毫秒) 
         **/ 
        typedef int64_t ATPTimeStampType;
        /**
         * @brief 时间 
         **/ 
        typedef int64_t ATPTimeType;
        /**
         * @brief 扩展字段类型 
         **/ 
        typedef char ATPExtraData[1001];
        /**
         * @brief 账户角色 
         **/ 
        typedef uint8_t ATPAccountRole;
        /**
         * @brief 证券类别 
         **/ 
        typedef uint16_t ATPSecurityTypeType;
        struct QUANT_API ATPSecurityTypeConst
        {
			//默认类型 
            static const ATPSecurityTypeType Default = 0;    
			//主板A股 
            static const ATPSecurityTypeType MainStockA = 1;    
			//核准制创业板股票 
            static const ATPSecurityTypeType GrowthStock = 3;    
			//主板B股 
            static const ATPSecurityTypeType MainStockB = 4;    
			//国债（含地方债） 
            static const ATPSecurityTypeType NationalBond = 5;    
			//企业债 
            static const ATPSecurityTypeType CorporateBond = 6;    
			//公司债 
            static const ATPSecurityTypeType CompanyBond = 7;    
			//可转债 
            static const ATPSecurityTypeType ConvertibleBond = 8;    
			//私募债 
            static const ATPSecurityTypeType PrivateBond = 9;    
			//可交换私募债 
            static const ATPSecurityTypeType ConvertiblePrivateBond = 10;    
			//证券公司次级债 
            static const ATPSecurityTypeType SecurityCompanySubBond = 11;    
			//质押式回购 
            static const ATPSecurityTypeType PledgeRepo = 12;    
			//资产支持证券 
            static const ATPSecurityTypeType ABS = 13;    
			//股票ETF 
            static const ATPSecurityTypeType StockETF = 14;    
			//跨市场股票ETF 
            static const ATPSecurityTypeType CMTStockETF = 15;    
			//跨境ETF 
            static const ATPSecurityTypeType CMTETF = 16;    
			//实物债券ETF 
            static const ATPSecurityTypeType PhysicalBondETF = 17;    
			//现金债券ETF 
            static const ATPSecurityTypeType CashBondETF = 18;    
			//黄金ETF 
            static const ATPSecurityTypeType GoldETF = 19;    
			//货币ETF 
            static const ATPSecurityTypeType CurrencyETF = 20;    
			//杠杆ETF 
            static const ATPSecurityTypeType LeverETF = 21;    
			//商品期货ETF 
            static const ATPSecurityTypeType CommodityFuturesETF = 22;    
			//标准LOF 
            static const ATPSecurityTypeType StandardLOF = 23;    
			//封闭式基金 
            static const ATPSecurityTypeType CloseFund = 25;    
			//仅申赎基金 
            static const ATPSecurityTypeType OnlyRedemptionFund = 26;    
			//配售权证 
            static const ATPSecurityTypeType RightsIssueWarrant = 27;    
			//权证 
            static const ATPSecurityTypeType Warrant = 28;    
			//个股期权 
            static const ATPSecurityTypeType StockOption = 29;    
			//ETF期权 
            static const ATPSecurityTypeType ETFOption = 30;    
			//资管产品 
            static const ATPSecurityTypeType FundManageProduct = 31;    
			//报价回购 
            static const ATPSecurityTypeType OfferRepo = 32;    
			//优先股 
            static const ATPSecurityTypeType PreferredStock = 33;    
			//证券公司短期债 
            static const ATPSecurityTypeType SecurityCompanyShortBond = 34;    
			//可交换公司债 
            static const ATPSecurityTypeType ConvertibleCompanyBond = 35;    
			//债券分销 
            static const ATPSecurityTypeType BondSell = 36;    
			//国债分销 
            static const ATPSecurityTypeType NationalBondSell = 37;    
			//其他债券 
            static const ATPSecurityTypeType OtherBond = 38;    
			//跨市场/跨境资金 
            static const ATPSecurityTypeType CMFund = 39;    
			//其他基金 
            static const ATPSecurityTypeType OtherFund = 40;    
			//国际版股票 
            static const ATPSecurityTypeType InternationalStock = 41;    
			//其他股票 
            static const ATPSecurityTypeType OtherStock = 42;    
			//买断式债券回购 
            static const ATPSecurityTypeType BuyoutBondRepo = 43;    
			//创业板可转债 
            static const ATPSecurityTypeType GrowthConvertibleBond = 44;    
			//存托凭证 
            static const ATPSecurityTypeType DepositoryReceipt = 45;    
			//沪伦通CDR 
            static const ATPSecurityTypeType SHLondonCDR = 46;    
			//创新企业股票 
            static const ATPSecurityTypeType InnovativeEnterpriseStock = 47;    
			//质押代码 
            static const ATPSecurityTypeType PledgeCode = 48;    
			//实时申赎货币基金 
            static const ATPSecurityTypeType RealTimeCurrencyFund = 49;    
			//新股新债发行 
            static const ATPSecurityTypeType StockBondIssue = 50;    
			//科创板Science and technology innovation board 
            static const ATPSecurityTypeType TIBStock = 51;    
			//港股 
            static const ATPSecurityTypeType HKStock = 52;    
			//科创板CDR 
            static const ATPSecurityTypeType TIBCDR = 53;    
			//期权股票 
            static const ATPSecurityTypeType OBSTOCK = 54;    
			//期权ETF 
            static const ATPSecurityTypeType OBETF = 55;    
			//注册制创业板股票 
            static const ATPSecurityTypeType RegistryGrowthStock = 56;    
			//注册制创新企业股票 
            static const ATPSecurityTypeType RegistryInnovativeEnterpriseStock = 57;    
			//注册制存托凭证 
            static const ATPSecurityTypeType RegistryDepositoryReceipt = 58;    
			//创业板创新企业股票 
            static const ATPSecurityTypeType ChinextInnovativeEnterpriseStock = 59;    
			//注册制创业板创新企业股票 
            static const ATPSecurityTypeType RegistryChinextInnovativeEnterpriseStock = 60;    
			//创业板存托凭证 
            static const ATPSecurityTypeType ChinextDepositoryReceipt = 61;    
			//注册制创业板存托凭证 
            static const ATPSecurityTypeType RegistryChinextDepositoryReceipt = 62;    
			//科创板ETF 
            static const ATPSecurityTypeType TIBETF = 63;    
			//科创板相关LOF 
            static const ATPSecurityTypeType TIBLOF = 64;    
			//基础设施基金 
            static const ATPSecurityTypeType InfrastructureFund = 65;    
			//定向可转债 
            static const ATPSecurityTypeType DirectionalConvertibleBonds = 66;    
			//科创板可转债 
            static const ATPSecurityTypeType TibConvertibleBond = 67;    
			//港股通ETF 
            static const ATPSecurityTypeType HKStocksThroughETF = 69;    
        };
        /**
         * @brief 字符串 
         **/ 
        typedef char ATPStringType[1001];
        /**
         * @brief 股份性质 
         **/ 
        typedef char ATPShareNegotiabilityType[3];
        struct QUANT_API ATPShareNegotiabilityConst
        {
			//流通股 
			static constexpr ATPShareNegotiabilityType CirculationStock = "00"; 
			//沪深：首发后限售股；北京：挂牌/上市后个人类限售股 
			static constexpr ATPShareNegotiabilityType RestrictedStock = "01"; 
			//股权激励限售股 
			static constexpr ATPShareNegotiabilityType IncentiveStock = "02"; 
			//挂牌/上市后机构类限售股 
			static constexpr ATPShareNegotiabilityType InstitutionStock = "03"; 
			//高管锁定股 
			static constexpr ATPShareNegotiabilityType ExecutiveLockStock = "04"; 
			//挂牌/上市前个人类限售股 
			static constexpr ATPShareNegotiabilityType PreHumanStock = "05"; 
			//挂牌/上市前机构类限售股 
			static constexpr ATPShareNegotiabilityType PreInstitutionStock = "06"; 
			//发起人国家股 
			static constexpr ATPShareNegotiabilityType InitiatorStateStock = "10"; 
			//发起人国有法人股 
			static constexpr ATPShareNegotiabilityType InitiatorSLS = "11"; 
			//发起人境内法人股 
			static constexpr ATPShareNegotiabilityType InitiatorDomesticStock = "12"; 
			//发起人外资法人股 
			static constexpr ATPShareNegotiabilityType InitiatorForeignStock = "13"; 
			//发起人自然人股 
			static constexpr ATPShareNegotiabilityType InitiatorNaturalStock = "14"; 
			//其他发起人股 
			static constexpr ATPShareNegotiabilityType OtherInitiatorStock = "15"; 
			//定向法人国家股 
			static constexpr ATPShareNegotiabilityType OrientationStateStock = "20"; 
			//定向法人国有法人股 
			static constexpr ATPShareNegotiabilityType OrientationSLS = "21"; 
			//定向法人境内法人股 
			static constexpr ATPShareNegotiabilityType OrientationDomesticStock = "22"; 
			//定向法人外资法人股 
			static constexpr ATPShareNegotiabilityType OrientationForeignStock = "23"; 
			//定向法人自然人股 
			static constexpr ATPShareNegotiabilityType OrientationNaturalStock = "24"; 
			//其他定向法人股 
			static constexpr ATPShareNegotiabilityType OtherOrientationStock = "25"; 
			//内部职工股 
			static constexpr ATPShareNegotiabilityType InternalStaffStock = "30"; 
			//高管股 
			static constexpr ATPShareNegotiabilityType ExecutiveStock = "40"; 
			//流通股待确认股份 
			static constexpr ATPShareNegotiabilityType CirculationUnconfirmedStock = "50"; 
			//限售股待确认股份 
			static constexpr ATPShareNegotiabilityType RestrictedUnconfirmedStock = "60"; 
        };
        /**
         * @brief 订单标识 
         **/ 
        typedef uint8_t ATPOrdSignType;
        struct QUANT_API ATPOrderSignConst
        {
			//委托订单 
            static const ATPOrdSignType kOrder = 0;    
			//撤单订单 
            static const ATPOrdSignType kCancel = 1;    
        };
        /**
         * @brief 订单编号 
         **/ 
        typedef char ATPOrderIDType[17];
        /**
         * @brief 执行编号,单个交易日不重复  
         **/ 
        typedef char ATPExecIDType[33];
        /**
         * @brief 信用标识 
         **/ 
        typedef char ATPCashMarginType;
        /**
         * @brief 特殊委托标识，按位表示特殊委托，0-正常委托，第一位表示夜市委托  
         **/ 
        typedef uint32_t ATPParticularFlagsType;
        /**
         * @brief 订单标识 
         **/ 
        typedef char ATPOrderFlagType;
        /**
         * @brief 终端识别码 
         **/ 
        typedef char ATPClientFeatureCodeType[1001];
        /**
         * @brief 申报状态详细信息 
         **/ 
        typedef char ATPOrdStatusInfoType[1001];
        /**
         * @brief 回调线程资源配置模式 
         **/ 
        typedef uint8_t ATPCallbackResourceModeType;
        struct QUANT_API ATPCallbackResourceModeConst
        {
			//低资源模式 
            static const ATPCallbackResourceModeType kLowResourceMode = 0;    
			//低时延模式 
            static const ATPCallbackResourceModeType kLowLatencyMode = 1;    
        };
        /**
         * @brief 线程模型枚举 
         **/ 
        typedef uint8_t ATPThreadModelType;
        struct QUANT_API ATPThreadModelConst
        {
			//独享 
            static const ATPThreadModelType kExclusived = 1;    
			//共享 
            static const ATPThreadModelType kShared = 2;    
        };
        /**
         * @brief 加密模式 
         **/ 
        typedef uint8_t ATPEncryptModeType;
        struct QUANT_API ATPEncryptModeConst
        {
			//透传模式 
            static const ATPEncryptModeType kTransMode = 0;    
			//加密模式 
            static const ATPEncryptModeType kEncryptMode = 1;    
        };
        /**
         * @brief 划拨方式 
         **/ 
        typedef uint8_t ATPTransferWayType;
        /**
         * @brief 执行类型 
         **/ 
        typedef char ATPExecTypeType;
        struct QUANT_API ATPExecTypeConst
        {
			//新订单 
            static const ATPExecTypeType New = '0';    
			//内部执行 
            static const ATPExecTypeType Internal = '1';    
			//已撤销 
            static const ATPExecTypeType Cancelled = '4';    
			//已拒绝 
            static const ATPExecTypeType Reject = '8';    
			//已成交 
            static const ATPExecTypeType Trade = 'F';    
			//黄金交易所成交 
            static const ATPExecTypeType Gold = 'J';    
        };
        /**
         * @brief 价格档位 
         **/ 
        typedef double ATPPriceTickType;
        /**
         * @brief bool类型 
         **/ 
        typedef bool ATPBoolType;
        /**
         * @brief 证券状态 
         **/ 
        typedef uint64_t ATPSecurityStatusType;
        /**
         * @brief 查询结果代码 
         **/ 
        typedef uint16_t ATPQueryResultCodeType;
        /**
         * @brief 密码校验服务错误码 
         **/ 
        typedef uint8_t ATPPermissonErrorCodeType;
        /**
         * @brief 外部实时指令拒绝代码 
         **/ 
        typedef uint16_t ExternalInsErrorCodeType;
        /**
         * @brief 拒绝代码 
         **/ 
        typedef uint16_t ATPRejectReasonCodeType;
        /**
         * @brief 订单拒绝原因代码 
         **/ 
        typedef char ATPOrdRejReasonType[6];
        /**
         * @brief 同步客户回报结束消息结果代码 
         **/ 
        typedef uint16_t ATPReasonCodeType;
        /**
         * @brief 协议类型 
         **/ 
        typedef uint8_t ATPConnectionProtocolType;
        struct QUANT_API ATPConnectionProtocolConst
        {
			//TCP协议 
            static const ATPConnectionProtocolType kTCPProtocol = 0;    
			//RUDP协议 
            static const ATPConnectionProtocolType kRUDPProtocol = 1;    
        };
        /**
         * @brief 申赎类型 
         **/ 
        typedef uint8_t ATPPrTypeType;
        struct QUANT_API ATPPrTypeConst
        {
			//无 
            static const ATPPrTypeType kNone = 0;    
			//普通申赎，指ETF所在市场的成分股允许实物申赎，其他市场的成分股采用现金替代的方式 
            static const ATPPrTypeType kGeneralPR = 1;    
			//场外实物申赎，指对深圳跨市场ETF，沪深成分股持仓组合允许实物申赎，非沪深成分股仍然使用现金替代的方式。该种申赎方式下，由中登对委托进行处理 
            static const ATPPrTypeType kPhysicalPR = 2;    
			//全实物申赎，指沪深成分股持仓组合允许实物申赎，港股、北交所等非沪深市场的成分股仍然使用现金替代的方式。该种申赎方式下，沪深两所通过建立实时数据交互机制，共同完成处理该申赎委托 
            static const ATPPrTypeType kAllPhysicalPR = 3;    
        };
        /**
         * @brief 现金替代标志 
         **/ 
        typedef uint8_t ATPSubstituteFlagType;
        struct QUANT_API ATPSubstituteFlagConst
        {
			//禁止 
            static const ATPSubstituteFlagType kBan = 0;    
			//允许 
            static const ATPSubstituteFlagType kAllow = 1;    
			//必须 
            static const ATPSubstituteFlagType kMust = 2;    
			//退补 
            static const ATPSubstituteFlagType kRefund = 3;    
        };
        /**
         * @brief 申购溢价比例N7(5) 
         **/ 
        typedef double ATPPremiumRatioType;
        /**
         * @brief ETF最大现金替代比例N6(5) 
         **/ 
        typedef double ATPMaxCashRatioType;
        /**
         * @brief 多通道推送自主订阅标志 
         **/ 
        typedef uint8_t ATPMultiChannelFlagType;
        struct QUANT_API ATPMultiChannelFlagConst
        {
			//默认值 
            static const ATPMultiChannelFlagType kDefault = 0xFF;    
			//主动取消订阅 
            static const ATPMultiChannelFlagType kActiveNoSubscribe = 0;    
			//主动订阅 
            static const ATPMultiChannelFlagType kActiveSubscribe = 1;    
        };
        /**
         * @brief 订阅结果 
         **/ 
        typedef uint8_t ATPMultiChannelResultType;
        struct QUANT_API ATPMultiChannelResultTypeConst
        {
			//已订阅 
            static const ATPMultiChannelResultType kSubscribed = 1;    
			//未订阅 
            static const ATPMultiChannelResultType kNoSubscribed = 2;    
			//未订阅，超订阅连接数上限 
            static const ATPMultiChannelResultType kOverSubscriptions = 3;    
			//未知，后台版本过低 
            static const ATPMultiChannelResultType kUnknow = 4;    
        };
        /**
         * @brief 报盘通道ID 
         **/ 
        typedef uint16_t ATPOpuIDType;
        /**
         * @brief 平台标识 
         **/ 
        typedef uint16_t ATPPlatformIDType;
        struct QUANT_API ATPPlatformIDConst
        {
			//上海：竞价撮合平台；深圳：现货集中竞价交易平台；北京：交易支持平台 
            static const ATPPlatformIDType kAuctionMatch = 1;    
			//上海：综合业务平台；深圳：综合金融服务平台 
            static const ATPPlatformIDType kFinance = 2;    
			//深圳：非交易处理平台 
            static const ATPPlatformIDType kNonMatch = 3;    
			//上海：上交所期权平台；深圳：衍生品集中竞价交易平台 
            static const ATPPlatformIDType kOption = 4;    
			//上海：上海港股通平台；深圳：国际互联互通平台 
            static const ATPPlatformIDType kHKThrough = 5;    
			//上海：新债券交易平台；深圳：固定收益交易平台 
            static const ATPPlatformIDType kFixedIncome = 6;    
			//上海：竞价撮合平台（流式）；深圳：场外业务平台 
            static const ATPPlatformIDType kAuctionNew = 7;    
			//上海：互联网交易平台 
            static const ATPPlatformIDType kInternel = 8;    
        };
        /**
         * @brief 备注 
         **/ 
        typedef char ATPRemarkType[1001];
        /**
         * @brief 外部系统统一划拨流水号 
         **/ 
        typedef int64_t ATPTransferSerialNoType;
        /**
         * @brief 外部系统银证转账流水号 
         **/ 
        typedef int64_t ATPBankTransferSerialNoType;
        /**
         * @brief 流水号 
         **/ 
        typedef int64_t ATPSerialNoType;
        /**
         * @brief 业务状态（ATP3.2.1开始支持） 
         **/ 
        typedef uint8_t ATPBusinessStatusType;
        /**
         * @brief 划拨数量 N15(2) 
         **/ 
        typedef double ATPShareTransferValueType;
        /**
         * @brief bool类型以char存储Y/N 
         **/ 
        typedef char ATPBoolCharType;
        /**
         * @brief 申购溢价比例N7(5) 
         **/ 
        typedef double ATPPremiumRatioType;
        /**
         * @brief ETF最大现金替代比例N6(5) 
         **/ 
        typedef double ATPMaxCashRatioType;
        /**
         * @brief 证券类型（字典值请咨询券商） 
         **/ 
        typedef char ATPSecurityTypeCharType[6];
        /**
         * @brief 资金属性类型 
         **/ 
        typedef char ATPFundPropertyType;
        struct QUANT_API ATPFundPropertyConst
        {
			//默认值 
            static const ATPFundPropertyType kDefault = ' ';    
			//T+0 
            static const ATPFundPropertyType kT0 = '0';    
			//T+1 
            static const ATPFundPropertyType kT1 = '1';    
        };
        /**
         * @brief 转出交易单元 
         **/ 
        typedef char ATPPbuIDType[7];
        /**
         * @brief 直接还券类型 
         **/ 
        typedef uint8_t ATPRedeliverType;
        struct QUANT_API ATPRedeliverTypeConst
        {
			//默认值 
            static const ATPRedeliverType Default = 255;    
			//标准还券 
            static const ATPRedeliverType Standard = 0;    
			//意向还券 
            static const ATPRedeliverType Will = 1;    
			//即时还券 
            static const ATPRedeliverType Realtime = 2;    
        };
        /**
         * @brief 实时划转标志,0:非实时划转,1:实时划转 
         **/ 
        typedef uint8_t ATPImmediateEffectType;
        struct QUANT_API ATPImmediateEffectConst
        {
			//非实时 
            static const ATPImmediateEffectType Standard = 0;    
			//实时 
            static const ATPImmediateEffectType Realtime = 1;    
        };
        /**
         * @brief 还款方式 
         **/ 
        typedef uint8_t ATPRepayWayType;
        struct QUANT_API ATPRepayWayConst
        {
			//默认值 
            static const ATPRepayWayType kDefault = 0xFF;    
			//直接还款 
            static const ATPRepayWayType kDirect = 0;    
			//意向还款 
            static const ATPRepayWayType kIntent = 1;    
        };
        /**
         * @brief 是否先用保底额度 
         **/ 
        typedef uint8_t ATPFirstUsePrivateAmountType;
        struct QUANT_API ATPFirstUsePrivateAmountConst
        {
			//否 
            static const ATPFirstUsePrivateAmountType No = 0;    
			//是 
            static const ATPFirstUsePrivateAmountType Yes = 1;    
			//未设置 
            static const ATPFirstUsePrivateAmountType Null = 2;    
        };
        /**
         * @brief 查询内容 
         **/ 
        typedef char ATPQueryContentsType;
        struct QUANT_API ATPQueryContentsConst
        {
			//默认值 
            static const ATPQueryContentsType kDefault = ' ';    
			//只查询余额 
            static const ATPQueryContentsType kBalance = '0';    
			//查询余额及明细 
            static const ATPQueryContentsType kBalanceDetail = '1';    
        };
        /**
         * @brief 分区号 
         **/ 
        typedef uint8_t ATPPartitionNoType;
        /**
         * @brief 了结标志 
         **/ 
        typedef char ATPCloseOutFlagType;
        struct QUANT_API ATPCloseOutFlagConst
        {
			//默认值 
            static const ATPCloseOutFlagType kDefault = ' ';    
			//全部 
            static const ATPCloseOutFlagType kAll = '0';    
			//未了结 
            static const ATPCloseOutFlagType kNotCloseOut = '1';    
			//已了结 
            static const ATPCloseOutFlagType kCloseOut = '2';    
			//全数未了结 
            static const ATPCloseOutFlagType kInitNotCloseOut = '3';    
			//部分未了结 
            static const ATPCloseOutFlagType kPartialNotCloseOut = '4';    
        };
        /**
         * @brief 融资融券信用标识 
         **/ 
        typedef uint8_t ATPCreditIdentificationType;
        struct QUANT_API ATPCreditIdentificationConst
        {
			//融资合约 
            static const ATPCreditIdentificationType kCrdBuyContract = 1;    
			//融券合约 
            static const ATPCreditIdentificationType kCrdSellContract = 2;    
			//融资利息 
            static const ATPCreditIdentificationType kCrdBuyInterest = 3;    
			//融券费用 
            static const ATPCreditIdentificationType kCrdSellFee = 4;    
			//资金权益补偿 
            static const ATPCreditIdentificationType kCapitalRightsCompensation = 5;    
			//股份权益补偿 
            static const ATPCreditIdentificationType kShareRightsCompensation = 6;    
			//逾期罚息 
            static const ATPCreditIdentificationType kOverdueInterest = 7;    
			//坏账罚息 
            static const ATPCreditIdentificationType kBadDebtInterest = 8;    
			//资金占用费 
            static const ATPCreditIdentificationType kCapitalOccupationFee = 9;    
			//管理费 
            static const ATPCreditIdentificationType kManagementFee = 10;    
        };
        /**
         * @brief 信用标识集合 
         **/ 
        typedef char ATPCashMarginSetType[41];
        /**
         * @brief 开仓流水号 
         **/ 
        typedef char ATPOpeningSerialNumType[33];
        /**
         * @brief 合同序号 
         **/ 
        typedef char ATPContractSerialNumType[25];
        /**
         * @brief 已展期次数 
         **/ 
        typedef uint32_t ATPRollOverNumberType;
        /**
         * @brief 利率产品 
         **/ 
        typedef uint8_t ATPInterestRateProductType;
        /**
         * @brief 保证金比例 
         **/ 
        typedef uint64_t ATPDepositRatioType;
        /**
         * @brief 融资融券标的类型 
         **/ 
        typedef uint16_t ATPCRDUnderlyingTypeType;
        struct QUANT_API ATPCRDUnderlyingTypeConst
        {
			//默认值 
            static const ATPCRDUnderlyingTypeType kDefault = 0xffff;    
			//全部-用于查询 
            static const ATPCRDUnderlyingTypeType kAll = 0;    
			//担保品 
            static const ATPCRDUnderlyingTypeType kCollateral = 1;    
			//融券 
            static const ATPCRDUnderlyingTypeType kSecuritiesLending = 2;    
			//融资 
            static const ATPCRDUnderlyingTypeType kMarginTrading = 3;    
        };
        /**
         * @brief 是否输出融券专户可用数量 
         **/ 
        typedef uint8_t ATPIsQueryMarginQtyType;
        struct QUANT_API ATPIsQueryMarginQtyConst
        {
			//不输出：记录都输出且融券专户可用数量默认0 
            static const ATPIsQueryMarginQtyType UnOutPut = 0;    
			//输出：记录都输出且融券专户可用数量输出实际计算值 
            static const ATPIsQueryMarginQtyType OutPut = 1;    
			//输出：可用数量大于0的记录都输出，可用数量为0的记录（当查询指定证券代码时输出记录，不指定证券代码时不输出） 
            static const ATPIsQueryMarginQtyType OutPutSpecial = 2;    
        };
        /**
         * @brief 标的类型 
         **/ 
        typedef char UnderlyingTypeType[33];
        /**
         * @brief 融资融券标的折算率N13(4) 
         **/ 
        typedef int64_t ATPCRDUnderlyingRateType;
        /**
         * @brief 融资融券业务开关 
         **/ 
        typedef char ATPCRDBusinessSwitchType[33];
        /**
         * @brief 交易所平台状态 
         **/ 
        typedef uint16_t ATPPlatformStatusType;
        struct QUANT_API ATPPlatformStatusConst
        {
			//未开放 
            static const ATPPlatformStatusType kUnopen = 0;    
			//即将开放 
            static const ATPPlatformStatusType kUpcomingOpening = 1;    
			//开放 
            static const ATPPlatformStatusType kOpen = 2;    
			//暂停 
            static const ATPPlatformStatusType kHalt = 3;    
			//关闭 
            static const ATPPlatformStatusType kClose = 4;    
        };
        /**
         * @brief 索引号 
         **/ 
        typedef int64_t ATPIndexType;
        /**
         * @brief 连接通道模式 
         **/ 
        typedef uint8_t ATPConnectChannelType;
        struct QUANT_API ATPConnectChannelConst
        {
			//软件连接 
            static const ATPConnectChannelType kSoftChannel = 0;    
			//硬件连接 
            static const ATPConnectChannelType kHardChannel = 1;    
        };
        /**
         * @brief 风控指标 
         **/ 
        typedef uint16_t ATPRiskControlIndicatorType;
        struct QUANT_API ATPRiskControlIndicatorConst
        {
			//申报速率 
            static const ATPRiskControlIndicatorType kRiskBusinessTypeOrderRate = 1022;    
			//当日委托笔数 
            static const ATPRiskControlIndicatorType kRiskBusinessTypeTodayOrderNumber = 1023;    
        };
        /**
         * @brief 计算维度 
         **/ 
        typedef uint8_t ATPCalculateDimensionType;
        struct QUANT_API ATPCalculateDimensionConst
        {
			//空 
            static const ATPCalculateDimensionType kNone = 0;    
			//所有客户 
            static const ATPCalculateDimensionType kALL = 1;    
			//单一客户 
            static const ATPCalculateDimensionType kSingle = 2;    
			//汇总客户 
            static const ATPCalculateDimensionType kSummarize = 3;    
			//枚举上限 
            static const ATPCalculateDimensionType kMax = 4;    
        };
        /**
         * @brief 系统设置的预警值N13(4) 
         **/ 
        typedef double ATPWarningValueType;
        /**
         * @brief 预警阈值N15(2) 
         **/ 
        typedef double ATPLimitValueType;
        /**
         * @brief 当前委撤笔数N13(4) 
         **/ 
        typedef double ATPOrderTotalCountType;
        /**
         * @brief 风控指标计算结果值N13(4) 
         **/ 
        typedef double ATPIndicatorValueType;
        /**
         * @brief 超禁止阈值后阻断范围 
         **/ 
        typedef uint16_t ATPLimitOperationType;
        /**
         * @brief 是否仅返回存在缺口的成分股信息类型 
         **/ 
        typedef uint8_t ATPETFQtyGapFlag;
        /**
         * @brief 投资组合 
         **/ 
        typedef char ATPShareUnitType[21];
        /**
         * @brief 交易员id 
         **/ 
        typedef char ATPTraderIdType[33];
        /**
         * @brief 基金通账户类型 
         **/ 
        typedef char ATPTradeAccountIdType[18];
        /**
         * @brief 委托方式扩展 
         **/ 
        typedef char ATPOrderWayExtType[33];
        /**
         * @brief ETF消息类型(ATP V4.6.5开始支持) 
         **/ 
        typedef char ATPETFTradeMsgType;
        struct QUANT_API ATPETFTradeMsgTypeConst
        {
			//非ETF申赎 
            static const ATPETFTradeMsgType kNoETF = ' ';    
			//ETF基金成交 
            static const ATPETFTradeMsgType kETFFund = '1';    
			//ETF成分券成交 
            static const ATPETFTradeMsgType kETFConstituent = '2';    
			//资金替代成交 
            static const ATPETFTradeMsgType kFundSubstitution = '3';    
        };
        /**
         * @brief 批量委托类型 
         **/ 
        typedef uint8_t ATPBatchTypeType;
        struct QUANT_API ATPBatchTypeConst
        {
			//默认值 
            static const ATPBatchTypeType kDefault = 0;    
			//单代码等量拆单 
            static const ATPBatchTypeType kEqual = 1;    
			//单代码递减拆单 
            static const ATPBatchTypeType kDecline = 2;    
			//多代码批量报单 
            static const ATPBatchTypeType kBatch = 3;    
        };
        /**
         * @brief 连接模式类型 
         **/ 
        typedef uint8_t ATPConnectionModeType;
        struct QUANT_API ATPConnectionModeConst
        {
			//直连模式 
            static const ATPConnectionModeType kDirect = 0;    
			//统一登录模式 
            static const ATPConnectionModeType kUnify = 1;    
        };
        /**
         * @brief 恢复结果类型 
         **/ 
        typedef uint8_t ATPRecoveredResultType;
        struct QUANT_API ATPRecoveredResultConst
        {
			//重连失败 
            static const ATPRecoveredResultType kRecoveredFailed = 0;    
			//重连成功 
            static const ATPRecoveredResultType kRecoveredSuccess = 1;    
        };
        /**
         * @brief 账户登出模式 
         **/ 
        typedef uint8_t ATPLogoutModeType;
        struct QUANT_API ATPLogoutModeConst
        {
			//客户号登出模式 
            static const ATPLogoutModeType kCustIDMode = 1;    
			//资金账号登出模式 
            static const ATPLogoutModeType kFundAccountIDMode = 2;    
        };
        /**
         * @brief 用户私有信息 
         **/ 
        typedef char ATPUserInfoType[17];
    } // namespace quant_api
} // namespace atp


#endif    //ATP_QUANT_CONSTANT_H_