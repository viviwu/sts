// code_generator_tag
// requests:a4c239568e897063270925a1ccc807fa responses:73a644cb24933bc89d6645e3fddcc719 platform:d260d3a6fe8792bf9f105e752cd594e1 template:c6b471fd40a56b1cc912fc720113769f code:6c46e513020b2bf45bdf5573afb15f32
/**
 * Copyright (c) 2018 Archforce Financial Technology.  All rights reserved.
 * Redistribution and use in source and binary forms, with or without  modification, are not permitted.
 * For more information about Archforce, welcome to archforce.cn.
 *
 * @file atp_quant_api.h
 * @author Archforce
 * @brief ATPQuantAPI
 *
 * @note ATPQuantAPI接口及回调句柄定义
 */

#ifndef ATP_QUANT_API_H_
#define ATP_QUANT_API_H_
#include "atp_quant_msg.h"


namespace atp
{
    namespace quant_api
    {
        class Context; //前置声明
        class QUANT_API ATPQuantHandler
        {
        public:
            /**
             * @brief 析构函数
             */
            virtual ~ATPQuantHandler() = default;
        public:
            /**
             * @brief 登录成功
             */
            virtual void OnLogin(const ATPCustomerInfo& msg) {}

            /**
             * @brief 登出成功
             */
            virtual void OnLogout(const char* desc) {}

            /**
             * @brief 账户异常登出
             */
            virtual void OnCustLogout(const ATPCustomerInfo& msg) {}

            /**
             * @brief 自动重连
             */
            virtual void OnRecovering(const char* desc) {}

             /**
             * @brief 重连结束
             */
            virtual void OnRecovered(const ATPRecoveredInfo& msg) {}

            /**
             * @brief 订阅平台状态信息结果
             */
            virtual void OnRspSubsPlatFormStateResult( const ATPRspErrorInfo& error_info, const int64_t request_id) {}
            
            /**
             * @brief 平台状态推送
             */
            virtual void OnRtnPlatFormStatePushResult(const ATPRtnPlatFormStatePushResultMsg& msg) {}
            
        public:
            /**
             * @brief 现货委托响应
             */
            virtual void OnRspCashAuctionOrder(const ATPRspCashAuctionOrderMsg& msg, const ATPRspErrorInfo& error_info, const int64_t request_id) {}

            /**
             * @brief 现货竞价批量委托响应
             */
            virtual void OnRspCashBatchAuctionOrder(const ATPRspCashBatchAuctionOrderMsg& msg, const ATPRspErrorInfo& error_info, const int64_t request_id) {}

            /**
             * @brief 现货委托回报
             */
            virtual void OnRtnCashAuctionOrder(const ATPRtnCashAuctionOrderMsg& msg) {}
            /**
             * @brief 现货撤单响应
             */
            virtual void OnRspCashCancelOrder(const ATPRspCashCancelOrderMsg& msg, const ATPRspErrorInfo& error_info, const int64_t request_id) {}

            /**
             * @brief 现货订单查询结果
             */
            virtual void OnRspCashOrderQueryResult(const ATPRspCashOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货股份查询结果
             */
            virtual void OnRspCashShareQueryResult(const ATPRspCashShareQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货按证券账户划转资金响应
             */
            virtual void OnRspCashExternalInsTETransFundResp(const ATPRspCashExternalInsTETransFundRespMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            /**
             * @brief 现货资金划拨(转入、转出)应答函数
             */
            virtual void OnRspCashExtFundTransferResult(const ATPRspCashExtFundTransferResultOtherMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            /**
             * @brief 现货成交查询结果
             */
            virtual void OnRspCashTradeOrderQueryResult(const ATPRspCashTradeOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货证券信息查询结果消息
             */
            virtual void OnRspCashExtQueryResultSecurityInfo(const ATPRspCashExtQueryResultSecurityInfoMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货资金查询结果
             */
            virtual void OnRspCashFundQueryResult(const ATPRspCashFundQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货资产查询结果
             */
            virtual void OnRspCashAssetQueryResult(const ATPRspCashAssetQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货成交汇总查询结果
             */
            virtual void OnRspCashTradeCollectQueryResult(const ATPRspCashCollectQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货最大可委托数查询结果
             */
            virtual void OnRspCashMaxOrderQueryResult(const ATPRspCashMaxOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief ETF信息查询结果
             */
            virtual void OnRspExtQueryResultETFInfo(const ATPRspExtQueryResultETFInfoMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief ETF成分股信息查询结果
             */
            virtual void OnRspExtQueryResultETFComponentInfo(const ATPRspExtQueryResultETFComponentInfoMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 客户报盘通道ID查询结果
             */
            virtual void OnRspOpuQueryResult(const ATPRspOpuQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 一步式银证转账响应
             */
            virtual void OnRspCashBankSecuritiesFundTransferResult(const ATPRspCashBankSecuritiesFundTransferResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            /**
             * @brief 外部持仓划拨响应
             */
            virtual void OnRspExtShareTransferResult(const ATPRspExtShareTransferResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            /**
             * @brief 新股中签结果查询结果
             */
            virtual void OnRspNewShareHitQueryResult(const ATPRspNewShareHitQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券委托响应
             */
            virtual void OnRspMarginOrder(const ATPRspMarginOrderMsg& msg, const ATPRspErrorInfo& error_info, const int64_t request_id) {}

            /**
             * @brief 融资融券委托回报
             */
            virtual void OnRtnMarginOrder(const ATPRtnMarginOrderMsg& msg) {}
            /**
             * @brief 融资融券直接还款应答消息
             */
            virtual void OnRspMarginExtRepayResult(const ATPRspExtRepayResultOtherMsg& msg, const ATPRspErrorInfo& error_info, const int64_t request_id) {}

            /**
             * @brief 融资融券撤单响应
             */
            virtual void OnRspMarginCancelOrder(const ATPRspMarginCancelOrderMsg& msg, const ATPRspErrorInfo& error_info, const int64_t request_id) {}

            /**
             * @brief 融资融券按证券账户划转资金响应
             */
            virtual void OnRspMarginExternalInsTETransFundResp(const ATPRspMarginExternalInsTETransFundRespMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            /**
             * @brief 融资融券资金划拨(转入、转出)应答函数
             */
            virtual void OnRspMarginExtFundTransferResult(const ATPRspMarginExtFundTransferResultOtherMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            /**
             * @brief 信用额度内部划拨响应
             */
            virtual void OnRspExternalInsTETransCreditAmountResp(const ATPRspExternalInsTETransCreditAmountRespMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            /**
             * @brief 融资保底额度内部划拨响应
             */
            virtual void OnRspExternalInsTETransCrdbuyAmountResp(const ATPRspExternalInsTETransCrdbuyAmountRespMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
            /**
             * @brief 融资融券股份查询结果
             */
            virtual void OnRspMarginShareQueryResult(const ATPRspMarginShareQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 信用账户可融资金查询结果
             */
            virtual void OnRspMarginTradingPrivateAmoutQueryResult(const ATPRspMarginTradingPrivateAmoutQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券订单查询结果
             */
            virtual void OnRspMarginOrderQueryResult(const ATPRspMarginOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券成交查询结果
             */
            virtual void OnRspMarginTradeOrderQueryResult(const ATPRspMarginTradeOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券资金查询结果
             */
            virtual void OnRspMarginFundQueryResult(const ATPRspMarginFundQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券最大可委托数查询结果
             */
            virtual void OnRspMarginMaxOrderQueryResult(const ATPRspMarginMaxOrderQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 信用账户可融数量查询结果
             */
            virtual void OnRspMarginQuerySecuritiesLendingPrivateAmountQueryResult(const ATPRspMarginQuerySecuritiesLendingPrivateAmountQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券保证金可用查询结果
             */
            virtual void OnRspQueryMarginUseableResult(const ATPRspMarginQueryMarginUseableQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券资金负债查询结果
             */
            virtual void OnRspMarginQueryFundLiabilityQueryResult(const ATPRspMarginQueryFundLiabilityQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券合约明细查询结果
             */
            virtual void OnRspMarginQueryContractSpecificationsQueryResult(const ATPRspMarginQueryContractSpecificationsQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券信用额度查询结果
             */
            virtual void OnRspMarginQueryCreditAmountQueryResult(const ATPRspMarginQueryCreditAmountQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券股份负债查询结果
             */
            virtual void OnRspMarginQueryShareLiabilityQueryResult(const ATPRspMarginQueryShareLiabilityQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 融资融券标的清单查询结果
             */
            virtual void OnRspMarginUnderlyingQueryResult(const ATPRspMarginUnderlyingQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货流量查询结果
             */
            virtual void OnRspCashOrderFlowQueryResult(const ATPRspCashOrderFlowQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货风控预警消息推送
             */
            virtual void OnRtnCashRiskControlWarning(const ATPRtnCashRiskControlWarningMsg& msg) {}

            /**
             * @brief ETF成分股客户持仓信息查询结果
             */
            virtual void OnRspETFComponentPositionQueryResult(const ATPRspExtETFComponentPositionQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) {}
            /**
             * @brief 现货外部实时资金查询结果
             */
            virtual void OnRspCashExtRealtimeFundQueryResult(const ATPRspCashExtRealtimeFundQueryResultMsg& msg, const int64_t request_id, const ATPRspErrorInfo& error_info) {}
        };

        class QUANT_API ATPQuantAPI
        {
        public:
            /**
             * @brief 构造函数
             *
             * @param[in]  handler    回调句柄
             * @param[in]  properties 配置参数
             *
             *              "locations"                             网关地址，const char*，必填，格式为"ip:port;ip:port"
             *              "order_way"                             委托方式，const char*，最长4位，必填
             *              "client_feature_code"                   终端识别码，const char*，必填
             *              "callback_resource_mode"                回调线程资源配置模式，ATPCallbackResourceModeType，非必填，默认值 kLowLatencyMode-低时延模式
             *              "recevie_thread_cpu"                    回调线程绑定CPU，uint8_t，非必填，默认值0xFF 表示不绑定
             *              "send_thread_cpu"                       发送线程绑定CPU，uint8_t，非必填，默认值0xFF 表示不绑定
             *              "is_tcp_direct"                         是否开启tcp_direct，bool，非必填，默认值false 表示不开启
             *              "bind_ip_address"                       绑定本地网卡地址，const char*，如果启用TcpDirect或RUDP协议，则该key值必填，默认值 空 表示不绑定
             *              "connection_protocol"                   协议类型，uint8_t，非必填，默认值kTCPProtocol-TCP协议
             *              "agw_user"                              网关用户名，const char*，非必填
             *                                                      网关用户名为空时表示匿名登录模式，Login只能登录一个客户号
             *                                                      网关用户名不为空时表示网关用户登录模式，可以多次调用Login登录多个客户号
             *              "agw_password"                          网关用户密码，const char*，非必填
             *              "retransmit_mode"                       重传模式，uint8_t，非必填，默认值kQuickMode-Quick模式 表示不重传历史RTN消息。1-Restart模式，重传所有历史RTN消息
             *              "multi_channel_flag"                    多通道自主订阅标志，uint8_t，默认值kDefault 表示以后台开关为准
             *              "connection_mode"                       连接模式，非必填，默认值0, 0:直连模式 1:统一接入模式
             */
            ATPQuantAPI(const ATPQuantHandler* handler, const ATPProperties* properties);

            /**
             * @brief 析构函数
             */
            ~ATPQuantAPI();

            /**
             * @brief 禁用拷贝构造
             */
            ATPQuantAPI(const ATPQuantAPI&) = delete;
            /**
             * @brief 禁用赋值构造
             */
            ATPQuantAPI& operator=(const ATPQuantAPI&) = delete;

            /**
             * @brief      初始化
             *
             * @note       只需要调用一次，非线程安全
             * @param[in]  properties 配置参数
             *
             *             "log_level"                            业务日志级别，ATPLogLevelType，非必填，默认值kInfo
             *             "common_log_path"                      业务日志路径，const char*，非必填，默认值 空 表示可执行文件当前目录的 ./log/quant_api_common_log_yyyymmdd.log
             *             "indicator_log_path"                   指标日志路径，const char*，非必填，默认值 空 表示可执行文件当前目录的 ./log/quant_api_indicator_log_yyyymmdd.log
             *             "io_log_path"                          IO日志路径，const char*，非必填，默认值 空 表示可执行文件当前目录的 ./log/quant_api_io_log_yyyymmdd.log
             *             "is_enable_latency"                    是否启用时延统计功能，bool，非必填，默认值false 表示不启用
             *
             * @return     ErrorCode 错误码
             */
            static ATPErrorCodeType  Init(const ATPProperties* properties);

            /**
             * @brief 停止
             *
             * @note 初始化成功后才能调用，调用之后会停止初始化中启动的资源和服务，之后API对象都将不能再发送消息，只能调用一次
             */
            static ATPErrorCodeType Stop();

            /**
             * @brief  登录接口
             * @param[in]  property  登录属性
             *
             * @return ErrorCode 错误码
             */
            ATPErrorCodeType  Login(const ATPLoginProperty* property);

            /**
             * @brief  登出接口
             * @return ErrorCode 错误码
             */
            ATPErrorCodeType  Logout();

            /**
             * @brief  释放资源
             * @return ErrorCode 错误码
             */
            ATPErrorCodeType  Release();

            /**
             * @brief       获取当前交易用户账号信息
             * @param[in]   const char* 客户号ID
             *              匿名连接模式下，返回当前连接登录的客户信息
             *              网关用户模式下，传入时返回对应的客户信息，未传入时返回最后一次登录成功的客户信息
             * @note        如果api未处于登录成功，且会话保持状态，则该函数返回空指针
             * @return      账号信息
             */
            ATPCustomerInfo* GetCustomerInfo(const char* cust_id = "") const;

            /**
             * @brief       获取API版本号
             * @return      版本信息
             */
            const char* GetVersion() const;

            /**
             * @brief       获取当前对象实例ID
             */
            int32_t GetInstanceId() const;

            /**
             * @brief       获取终端识别码
             *
             * @return      处理完成的终端识别码
             */
            static const char* GetClientFeatureCode(const ClientFeatureCode* client_feature_code);

            /**
             * @brief       获取当前用户连接信息
             *
             * @return      用户连接信息
             */
            ATPConnectionInfo* GetConnectionInfo() const;

            /**
             * @brief       订阅平台状态信息
             *
             * @return      ErrorCode 错误码
             */
            ATPErrorCodeType ReqSubsPlatFormState(int64_t request_id);

            /**
             * @brief T1现货通用下单
             */
            ATPErrorCodeType ReqCashAuctionOrderFast(const ATPReqCashAuctionOrderMsg* msg, const int64_t request_id);

            /**
             * @brief T1现货通用撤单
             */
            ATPErrorCodeType ReqCashCancelOrderFast(const ATPReqCashCancelOrderMsg* msg, const int64_t request_id);
            
            /**
            * @brief       客户登出接口
            * @details     仅登出账户信息，不断开连接
            *
            * @param[in]   property 登出消息（需预先填充用户标识等必要信息）
            *
            * @return      ErrorCode 错误码
            * @note        调用前需确保已通过登录接口成功获取会话，否则会返回错误码
            * @sa          Login, ATPLogoutProperty
            */
            ATPErrorCodeType CustLogout(const ATPLogoutProperty* property);
            
        public:
            /**
             * @brief 现货通用下单
             */
            ATPErrorCodeType ReqCashAuctionOrder(const ATPReqCashAuctionOrderMsg* msg, const int64_t request_id);
            /**
             * @brief 现货通用撤单
             */
            ATPErrorCodeType ReqCashCancelOrder(const ATPReqCashCancelOrderMsg* msg, const int64_t request_id);
            /**
             * @brief 现货订单查询
             */
            ATPErrorCodeType ReqCashOrderQuery(const ATPReqCashOrderQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货批量委托
             */
            ATPErrorCodeType ReqCashBatchAuctionOrder(const ATPReqBatchCashAuctionOrderMsg* msg, const int64_t request_id);
            /**
             * @brief 现货股份查询
             */
            ATPErrorCodeType ReqCashShareQuery(const ATPReqCashShareQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货按证券账户划转资金
             */
            ATPErrorCodeType ReqCashExternalInsTETransFundReq(const ATPReqCashExternalInsTETransFundReqMsg* msg, const int64_t request_id);
            /**
             * @brief 现货资金划拨
             */
            ATPErrorCodeType ReqCashExtFundTransferOther(const ATPReqCashExtFundTransferOtherMsg* msg, const int64_t request_id);
            /**
             * @brief 现货成交查询
             */
            ATPErrorCodeType ReqCashTradeOrderQuery(const ATPReqCashTradeOrderQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货证券信息查询消息
             */
            ATPErrorCodeType ReqCashExtQuerySecurityInfo(const ATPReqCashExtQuerySecurityInfoMsg* msg, const int64_t request_id);
            /**
             * @brief 现货资金查询
             */
            ATPErrorCodeType ReqCashFundQuery(const ATPReqCashFundQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货资产查询
             */
            ATPErrorCodeType ReqCashAssetQuery(const ATPReqCashAssetQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货成交汇总查询
             */
            ATPErrorCodeType ReqCashTradeCollectQuery(const ATPReqCashTradeCollectQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货最大可委托数查询
             */
            ATPErrorCodeType ReqCashMaxOrderQtyQuery(const ATPReqCashMaxOrderQtyQueryMsg* msg, const int64_t request_id);
            /**
             * @brief ETF信息查询
             */
            ATPErrorCodeType ReqExtQueryETFInfo(const ATPReqExtQueryETFInfoMsg* msg, const int64_t request_id);
            /**
             * @brief ETF成分股信息查询
             */
            ATPErrorCodeType ReqExtQueryETFComponentInfo(const ATPReqExtQueryETFComponentInfoMsg* msg, const int64_t request_id);
            /**
             * @brief 客户报盘通道ID查询
             */
            ATPErrorCodeType ReqOpuQuery(const ATPReqOpuQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 一步式银证转账
             */
            ATPErrorCodeType ReqCashBankSecuritiesFundTransferOther(const ATPReqCashBankSecuritiesFundTransferOtherMsg* msg, const int64_t request_id);
            /**
             * @brief 外部持仓划拨
             */
            ATPErrorCodeType ReqExtShareTransfer(const ATPReqExtShareTransferMsg* msg, const int64_t request_id);
            /**
             * @brief 新股中签结果查询
             */
            ATPErrorCodeType ReqNewShareHitQuery(const ATPReqNewShareHitQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券通用委托
             */
            ATPErrorCodeType ReqMarginOrder(const ATPReqMarginOrderMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券直接还款
             */
            ATPErrorCodeType ReqMarginExtRepay(const ATPReqMarginExtRepayMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券通用撤单
             */
            ATPErrorCodeType ReqMarginCancelOrder(const ATPReqMarginCancelOrderMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券按证券账户划拨资金
             */
            ATPErrorCodeType ReqMarginExternalInsTETransFundReq(const ATPReqMarginExternalInsTETransFundReqMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券外部资金划拨
             */
            ATPErrorCodeType ReqMarginExtFundTransferOther(const ATPReqMarginExtFundTransferOtherMsg* msg, const int64_t request_id);
            /**
             * @brief 信用额度内部划拨
             */
            ATPErrorCodeType ReqExternalInsTETransCreditAmountReq(const ATPReqExternalInsTETransCreditAmountReqMsg* msg, const int64_t request_id);
            /**
             * @brief 融资保底额度内部划拨
             */
            ATPErrorCodeType ReqExternalInsTETransCrdbuyAmountReq(const ATPReqExternalInsTETransCrdbuyAmountReqMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券股份查询
             */
            ATPErrorCodeType ReqMarginShareQuery(const ATPReqMarginShareQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 信用账户可融资金查询
             */
            ATPErrorCodeType ReqMarginTradingAvailableAmtQuery(const ATPReqMarginTradingAvailableAmtQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券订单查询
             */
            ATPErrorCodeType ReqMarginOrderQuery(const ATPReqMarginOrderQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券成交查询
             */
            ATPErrorCodeType ReqMarginTradeOrderQuery(const ATPReqMarginTradeOrderQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券资金查询
             */
            ATPErrorCodeType ReqMarginFundQuery(const ATPReqMarginFundQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券最大可委托数查询
             */
            ATPErrorCodeType ReqMarginMaxOrderQtyQuery(const ATPReqMarginMaxOrderQtyQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 信用账户可融数量查询
             */
            ATPErrorCodeType ReqMarginTradingAvailableQtyQuery(const ATPReqMarginTradingAvailableQtyQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券保证金可用查询
             */
            ATPErrorCodeType ReqExtQueryMarginUseable(const ATPReqMarginExtQueryMarginUseableMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券资金负债查询
             */
            ATPErrorCodeType ReqMarginExtQueryFundLiability(const ATPReqMarginExtQueryFundLiabilityMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券合约明细查询
             */
            ATPErrorCodeType ReqMarginExtQueryContractSpecifications(const ATPReqMarginExtQueryContractSpecificationsMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券信用额度查询
             */
            ATPErrorCodeType ReqMarginExtQueryCreditAmount(const ATPReqMarginExtQueryCreditAmountMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券股份负债查询
             */
            ATPErrorCodeType ReqMarginExtQueryShareLiability(const ATPReqMarginExtQueryShareLiabilityMsg* msg, const int64_t request_id);
            /**
             * @brief 融资融券标的清单查询
             */
            ATPErrorCodeType ReqMarginExtQueryMarginUnderlying(const ATPReqMarginExtQueryMarginUnderlyingMsg* msg, const int64_t request_id);
            /**
             * @brief 现货流量查询
             */
            ATPErrorCodeType ReqCashOrderFlowQuery(const ATPReqCashOrderFlowQueryMsg* msg, const int64_t request_id);
            /**
             * @brief ETF成分股客户持仓信息查询
             */
            ATPErrorCodeType ReqETFComponentPositionQuery(const ATPReqExtETFComponentPositionQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货外部实时资金查询
             */
            ATPErrorCodeType ReqCashExtRealtimeFundQuery(const ATPReqCashExtRealtimeFundQueryMsg* msg, const int64_t request_id);
            /**
             * @brief 现货竞价批量委托撤单
             */
            ATPErrorCodeType ReqCashCancelBatchOrder(const ATPReqCashCancelBatchOrderMsg* msg, const int64_t request_id);

        private:
            atp::quant_api::Context* context_;
            friend class ATPReqCashAuctionOrderMsg;
            friend class ATPReqCashCancelOrderMsg;

        };
    } //namespace quant_api
} //namespace atp




#endif    //ATP_QUANT_API_H_