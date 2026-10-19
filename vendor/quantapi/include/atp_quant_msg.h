// code_generator_tag
// requests:a4c239568e897063270925a1ccc807fa responses:73a644cb24933bc89d6645e3fddcc719 constants:f7efecb9dc2e6aa8eed433c797d5e05b types:4b1c6ac864222617122ef3765f571413 trade_messages:12e838c87e988e51995fa415a0a7be9a platform:d260d3a6fe8792bf9f105e752cd594e1 template:0557ee31dfeb282e9735187241167c9c code:107bc4029d73afe5179875eb6c652db6
/**
 * Copyright (c) 2018 Archforce Financial Technology.  All rights reserved.
 * Redistribution and use in source and binary forms, with or without  modification, are not permitted.
 * For more information about Archforce, welcome to archforce.cn.
 *
 * @file atp_quant_msg.h
 * @author Archforce
 * @brief message
 *
 * @note ATPQuantAPI消息定义
 */
#ifndef ATP_QUANT_MSG_H_
#define ATP_QUANT_MSG_H_
#include "atp_quant_constant.h"

//前置声明
namespace atp
{
    namespace api
    {
        class PropertiesImpl;
    }
}

namespace atp
{
    namespace quant_api
    {
        class ATPQuantAPI;
        /**
         * @brief 二进制数据流结构
         */
        class QUANT_API Buffer
        {
        public:
            Buffer() : data_size(0), data(nullptr) {}

            Buffer(const uint32_t data_size, const char* data) : data_size(data_size), data(data) {}

        public:
            uint32_t data_size;     // 二进制数据流长度
            const char* data;       // 二进制数据流首地址
        };

        /**
         * @brief 配置属性类
         */
        class QUANT_API ATPProperties
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPProperties();

            /**
             * @brief 析构函数
             */
            ~ATPProperties();

            ATPProperties(const ATPProperties&) = delete;

            ATPProperties& operator=(const ATPProperties&) = delete;

            void SetValue(const char* key, const char* value);

            void SetValue(const char* key, bool value);

            void SetValue(const char* key, char value);

            void SetValue(const char* key, uint8_t value);

            void SetValue(const char* key, int8_t value);

            void SetValue(const char* key, uint16_t value);

            void SetValue(const char* key, int16_t value);

            void SetValue(const char* key, uint32_t value);

            void SetValue(const char* key, int32_t value);

            void SetValue(const char* key, uint64_t value);

            void SetValue(const char* key, int64_t value);

            const char* GetValue(const char* key, const char* default_value) const;

            bool GetValue(const char* key, bool default_value) const;

            char GetValue(const char* key, char default_value) const;

            uint8_t GetValue(const char* key, uint8_t default_value) const;

            int8_t GetValue(const char* key, int8_t default_value) const;

            uint16_t GetValue(const char* key, uint16_t default_value) const;

            int16_t GetValue(const char* key, int16_t default_value) const;

            uint32_t GetValue(const char* key, uint32_t default_value) const;

            int32_t GetValue(const char* key, int32_t default_value) const;

            uint64_t GetValue(const char* key, uint64_t default_value) const;

            int64_t GetValue(const char* key, int64_t default_value) const;

            bool HasKey(const char* key) const;

        private:
            atp::api::PropertiesImpl* impl_;
        };

        /**
         * @brief 错误信息
         */
        class QUANT_API ATPRspErrorInfo
        {
        public:
            /**
             * @brief 构造方法
             */
            ATPRspErrorInfo() : error_id(0) {}

            /**
             * @brief 析构方法
             */
            virtual ~ATPRspErrorInfo() = default;

            /**
             * @brief 获取错误描述
             */
            virtual const char* GetErrorMsg() const = 0;

        public:
            int32_t error_id;                //错误码
        };

        /**
         * @brief 终端信息
         */
        class QUANT_API ClientFeatureCode
        {
        public:
            /**
             * @brief 构造方法
             */
            ClientFeatureCode() = default;

            /**
             * @brief 析构方法
             */
            virtual ~ClientFeatureCode() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ClientFeatureCode(const ClientFeatureCode&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ClientFeatureCode& operator=(const ClientFeatureCode&) = delete;

            /**
             * @brief 构建消息
             */
            static ClientFeatureCode* NewMessage();

            /**
             * @brief 销毁消息
             */
            static void DeleteMessage(ClientFeatureCode* ptr);

        public:
            //终端类型
            virtual void SetTerminalType(const char* terminal_type) = 0;
            virtual const char* GetTerminalType() const = 0;
            //公网IP地址
            virtual void SetIip(const char* iip) = 0;
            virtual const char* GetIip() const = 0;
            //公网端口
            virtual void SetIport(const char* iport) = 0;
            virtual const char* GetIport() const = 0;
            //内网IP地址
            virtual void SetLip(const char* lip) = 0;
            virtual const char* GetLip() const = 0;
            //MAC地址
            virtual void SetMac(const char* mac) = 0;
            virtual const char* GetMac() const = 0;
            //硬盘序列号
            virtual void SetHd(const char* hd) = 0;
            virtual const char* GetHd() const = 0;
            //PC终端设备名
            virtual void SetPcn(const char* pcn) = 0;
            virtual const char* GetPcn() const = 0;
            //CPU序列号
            virtual void SetCpu(const char* cpu) = 0;
            virtual const char* GetCpu() const = 0;
            //硬盘分区信息
            virtual void SetPi(const char* pi) = 0;
            virtual const char* GetPi() const = 0;
            //系统盘卷标号
            virtual void SetVol(const char* vol) = 0;
            virtual const char* GetVol() const = 0;
            //交易终端软件名称及版本
            virtual void SetTername(const char* tername) = 0;
            virtual const char* GetTername() const = 0;
        };

        /**
         * @brief 登录消息
         */
        class QUANT_API ATPLoginProperty
        {
        public:
            /**
             * @brief 构造方法
             */
            ATPLoginProperty() = default;

            /**
             * @brief 析构方法
             */
            virtual ~ATPLoginProperty() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPLoginProperty(const ATPLoginProperty&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPLoginProperty& operator=(const ATPLoginProperty&) = delete;

            /**
             * @brief 构建消息
             */
            static ATPLoginProperty* NewMessage();

            /**
             * @brief 销毁消息
             */
            static void DeleteMessage(ATPLoginProperty* ptr);

        public:
            //账户ID 客户号登录模式下填客户号，资金账号登录模式下填资金账号
            virtual void SetUserId(const char* user_id) = 0;
            virtual const char* GetUserId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char* branch_id) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户密码
            virtual void SetPassword(const char* password) = 0;
            virtual const char* GetPassword() const = 0;
            //登录模式
            virtual void SetLoginMode(uint8_t login_mode) = 0;
            virtual uint8_t GetLoginMode() const = 0;
            //扩展字段
            virtual void SetExtraData(const char* extra_data) = 0;
            virtual const char* GetExtraData() const = 0;
            //连接通道
            virtual void SetConnectChannel(uint8_t connect_channel) = 0;
            virtual uint8_t GetConnectChannel() const = 0;
        };

        /**
         * @brief 交易用户信息
         */
        class QUANT_API ATPCustomerInfo
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPCustomerInfo() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPCustomerInfo() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPCustomerInfo(const ATPCustomerInfo&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPCustomerInfo& operator=(const ATPCustomerInfo&) = delete;

        public:
            //客户号
            virtual const char* GetCustId() const = 0;
            //是否为多分区账号
            virtual uint8_t GetIsMultiPartitions() const = 0;
            //资金账号重复组大小
            virtual uint32_t FundAccountArraySize() const = 0;
            //资金账号
            virtual const char* FundAccountArray_GetFundAccountId(uint32_t index) const = 0;
            //营业部ID
            virtual const char* FundAccountArray_GetBranchId(uint32_t index) const = 0;
            //证券账号重复组大小
            virtual uint32_t FundAccountArray_AccountArraySize(uint32_t index) const = 0;
            //证券账号
            virtual const char* FundAccountArray_AccountArray_GetAccountId(uint32_t index1, uint32_t index2) const = 0;
            //市场代码
            virtual uint16_t FundAccountArray_AccountArray_GetMarketId(uint32_t index1, uint32_t index2) const = 0;
            //账户角色
            virtual uint8_t FundAccountArray_AccountArray_GetAccountRole(uint32_t index1, uint32_t index2) const = 0;
            //所属分区号
            virtual uint8_t FundAccountArray_AccountArray_GetPartitionNo(uint32_t index1, uint32_t index2) const = 0;
            //连接通道
            virtual uint8_t GetConnectChannel() const = 0;
        };

        /**
         * @brief 连接信息
         */
        class QUANT_API ATPConnectionInfo
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPConnectionInfo() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPConnectionInfo() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPConnectionInfo(const ATPConnectionInfo&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPConnectionInfo& operator=(const ATPConnectionInfo&) = delete;

        public:
            //实例ID
            virtual int32_t GetInstanceId() const = 0;
            //多通道主推消息订阅结果
            virtual uint8_t GetMultiChannelResult() const = 0;
            //连接通道
            virtual uint8_t GetConnectChannel() const = 0;
        };

        /**
         * @brief 恢复结束信息
         */
        class QUANT_API ATPRecoveredInfo
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRecoveredInfo() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRecoveredInfo() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRecoveredInfo(const ATPRecoveredInfo&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRecoveredInfo& operator=(const ATPRecoveredInfo&) = delete;

        public:
            //分区号
            virtual int32_t GetPartitionNo() const = 0;
            //重连结果
            virtual uint8_t GetRecoveredResult() const = 0;
        };


        /**
         * @brief 平台状态推送消息
         */
        class QUANT_API ATPRtnPlatFormStatePushResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRtnPlatFormStatePushResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRtnPlatFormStatePushResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRtnPlatFormStatePushResultMsg(const ATPRtnPlatFormStatePushResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRtnPlatFormStatePushResultMsg& operator=(const ATPRtnPlatFormStatePushResultMsg&) = delete;
        public:
            //交易市场
            virtual uint16_t GetMarketId() const = 0;
            //交易所平台标识
            virtual uint16_t GetPlatform() const = 0;
            //平台状态
            virtual uint16_t GetPlatformStatus() const = 0;
        };

        /**
         * @brief 登出消息
         */
        class QUANT_API ATPLogoutProperty
        {
        public:
            /**
             * @brief 构造方法
             */
            ATPLogoutProperty() = default;

            /**
             * @brief 析构方法
             */
            virtual ~ATPLogoutProperty() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPLogoutProperty(const ATPLogoutProperty&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPLogoutProperty& operator=(const ATPLogoutProperty&) = delete;

            /**
             * @brief 构建消息
             */
            static ATPLogoutProperty* NewMessage();

            /**
             * @brief 销毁消息
             */
            static void DeleteMessage(ATPLogoutProperty* ptr);

        public:
            //账户ID 客户号登录模式下填客户号，资金账号登录模式下填资金账号
            virtual void SetUserId(const char* user_id) = 0;
            virtual const char* GetUserId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char* branch_id) = 0;
            virtual const char* GetBranchId() const = 0;
            //登出模式
            virtual void SetLogoutMode(uint8_t logout_mode) = 0;
            virtual uint8_t GetLogoutMode() const = 0;
        };


        /**
         * @brief 现货交易委托消息
         */
        class QUANT_API ATPReqCashAuctionOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashAuctionOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashAuctionOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashAuctionOrderMsg(const ATPReqCashAuctionOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashAuctionOrderMsg& operator=(const ATPReqCashAuctionOrderMsg&) = delete;
            /**
             * @brief 构建消息
             * @param business_type 业务类型
             */
            static ATPReqCashAuctionOrderMsg* NewMessage(ATPBusinessTypeType business_type);
            /**
             * @brief 构建零拷贝消息
             * @note 仅支持接入T1时使用
             * @note 必须在构建API对象后才能调用该接口，与API对象绑定，其他API对象无法调用该消息；该消息不能复用，每次发送前需要构造消息。
             */
            static ATPReqCashAuctionOrderMsg* NewMessageFast(ATPQuantAPI* client);
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashAuctionOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账号ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //证券代码 （当为配股业务时，该字段表示配售权证代码）
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //市场代码 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //买卖方向 
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //申报数量N15(2) 
            virtual void SetOrderQty(double) = 0;
            virtual double GetOrderQty() const = 0;
            //委托价格N13(4) （上海市场股票、存托凭证、基金市价委托必须输入保护价） 
            virtual void SetPrice(double) = 0;
            virtual double GetPrice() const = 0;
            //订单类型 
            virtual void SetOrderType(char) = 0;
            virtual char GetOrderType() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用)（ATP3.2.3开始支持现货集中竞价；ATP3.2.18开始支持ETF申赎）
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //报盘通道ID
            virtual void SetOpuId(uint16_t) = 0;
            virtual uint16_t GetOpuId() const = 0;
            //联动市场。对支持全实物申赎的ETF有效，上海跨市场股票ETF的联动市场为深圳，深圳跨市场股票ETF的联动市场为上海
            virtual void SetRelatedMarketId(uint16_t) = 0;
            virtual uint16_t GetRelatedMarketId() const = 0;
            //联动市场证券账户。对支持全实物申赎的ETF有效，同资金账户下联动市场存在多个证券账户时必填。
            virtual void SetRelatedAccountId(const char*) = 0;
            virtual const char* GetRelatedAccountId() const = 0;
            //委托方式
            virtual void SetOrderWay(const char*) = 0;
            virtual const char* GetOrderWay() const = 0;
            //股份性质
            virtual void SetShareNegotiability(const char*) = 0;
            virtual const char* GetShareNegotiability() const = 0;
            //用户私有信息
            virtual void SetUserInfo(const char*) = 0;
            virtual const char* GetUserInfo() const = 0;
        };

        /**
         * @brief 现货订单委托响应
         */
        class QUANT_API ATPRspCashAuctionOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashAuctionOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashAuctionOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashAuctionOrderMsg(const ATPRspCashAuctionOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashAuctionOrderMsg& operator=(const ATPRspCashAuctionOrderMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashAuctionOrderMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashAuctionOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //客户自定义委托批号（ATP3.2.3开始支持现货集中竞价；ATP3.2.18开始支持ETF申赎）
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //委托价格N13(4)
            virtual double GetPrice() const = 0;
            //委托数量N15(2)
            virtual double GetOrderQty() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //订单类型
            virtual char GetOrderType() const = 0;
            //响应时间
            virtual int64_t GetTransactTime() const = 0;
            //用户私有信息
            virtual const char* GetUserInfo() const = 0;
        };

        /**
         * @brief 现货撤单委托响应
         */
        class QUANT_API ATPRspCashCancelOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashCancelOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashCancelOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashCancelOrderMsg(const ATPRspCashCancelOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashCancelOrderMsg& operator=(const ATPRspCashCancelOrderMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashCancelOrderMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashCancelOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //原委托的客户订单编号
            virtual int64_t GetOrigClOrdNo() const = 0;
            //响应时间
            virtual int64_t GetTransactTime() const = 0;
            //客户自定义委托批号，指示被撤消订单的batch_cl_ord_no。（ATP3.2.3开始支持，且仅支持现货集中竞价）
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //用户私有信息
            virtual const char* GetUserInfo() const = 0;
        };

        /**
         * @brief 现货主推消息
         */
        class QUANT_API ATPRtnCashAuctionOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRtnCashAuctionOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRtnCashAuctionOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRtnCashAuctionOrderMsg(const ATPRtnCashAuctionOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRtnCashAuctionOrderMsg& operator=(const ATPRtnCashAuctionOrderMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRtnCashAuctionOrderMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRtnCashAuctionOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账号ID 
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual const char* GetAccountId() const = 0;
            //业务类型 
            virtual uint8_t GetBusinessType() const = 0;
            //客户订单编号 
            virtual int64_t GetClOrdNo() const = 0;
            //客户自定义委托批号（ATP3.2.3开始支持现货集中竞价；ATP3.2.18开始支持ETF申赎）
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //证券代码 
            virtual const char* GetSecurityId() const = 0;
            //市场代码 
            virtual uint16_t GetMarketId() const = 0;
            //订单状态 
            virtual uint8_t GetOrdStatus() const = 0;
            //订单标识 
            virtual uint8_t GetOrdSign() const = 0;
            //委托价格N13(4) 
            virtual double GetPrice() const = 0;
            //委托数量N15(2) 
            virtual double GetOrderQty() const = 0;
            //未成交部分的数量N15(2) 
            virtual double GetLeavesQty() const = 0;
            //累计成交数量N15(2) 
            virtual double GetCumQty() const = 0;
            //买卖方向 
            virtual char GetSide() const = 0;
            //回报时间 
            virtual int64_t GetTransactTime() const = 0;
            //交易所订单编号 
            virtual const char* GetOrderId() const = 0;
            //申报合同号 
            virtual const char* GetClOrdId() const = 0;
            //冻结交易金额N15(4)
            virtual double GetFrozenTradeValue() const = 0;
            //冻结费用N15(4)
            virtual double GetFrozenFee() const = 0;
            //执行编号（此字段仅对交易成交回报有效，其余消息均为空）
            virtual const char* GetExecId() const = 0;
            //确认编号(此字段仅对深交所确认回报有效，取值自深交所确认执行报告的ExecID;其余消息均为空）
            virtual const char* GetConfirmExecId() const = 0;
            //成交价格N13(4)
            virtual double GetLastPx() const = 0;
            //成交数量N15(2)
            virtual double GetLastQty() const = 0;
            //成交金额N15(4)
            virtual double GetTotalValueTraded() const = 0;
            //ETF成交回报类型
            virtual uint8_t GetEtfTradeReportType() const = 0;
            //信用标识
            virtual char GetCashMargin() const = 0;
            //到期日
            virtual int64_t GetMaturityDate() const = 0;
            //原委托的客户订单编号
            virtual int64_t GetOrigClOrdNo() const = 0;
            //母单订单编号
            virtual int64_t GetParentClOrdNo() const = 0;
            //拒绝原因代码
            virtual int32_t GetRejectReasonCode() const = 0;
            //拒绝原因描述
            virtual const char* GetOrdRejReason() const = 0;
            //报盘通道ID
            virtual uint16_t GetOpuId() const = 0;
            //成分股与资金记录总数 
            virtual int64_t GetEtfReportComponentNum() const = 0;
            //成交回报类型（ATP4.6.3开始支持）0-普通成交、1-联动市场成交
            virtual uint8_t GetTradeReportType() const = 0;
            //ETF消息类型
            virtual char GetEtfTradeMsgType() const = 0;
            //用户私有信息
            virtual const char* GetUserInfo() const = 0;
        };

        /**
         * @brief 现货通用撤单消息
         */
        class QUANT_API ATPReqCashCancelOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashCancelOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashCancelOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashCancelOrderMsg(const ATPReqCashCancelOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashCancelOrderMsg& operator=(const ATPReqCashCancelOrderMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashCancelOrderMsg* NewMessage();
            /**
             * @brief 构建零拷贝消息
             * @note 仅支持接入T1时使用
             * @note 必须在构建API对象后才能调用该接口，与API对象绑定，其他API对象无法调用该消息；该消息不能复用，每次发送前需要构造消息。
             */
            static ATPReqCashCancelOrderMsg* NewMessageFast(ATPQuantAPI* client);
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashCancelOrderMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //原始交易客户方（券商）订单编号，指示被撤消订单的cl_ord_no（ATP3.2.3开始支持现货集中竞价非必填，其他业务必填）
            virtual void SetOrigClOrdNo(int64_t) = 0;
            virtual int64_t GetOrigClOrdNo() const = 0;
            //客户自定义委托批号，指示被撤消订单的batch_cl_ord_no。batch_cl_ord_no与orig_cl_ord_no至少填写一项。若orig_cl_ord_no不为0，则按orig_cl_ord_no撤单；否则，batch_cl_ord_no不为0，按batch_cl_ord_no撤单（ATP3.2.3开始支持，且仅支持现货集中竞价）
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //报盘通道ID
            virtual void SetOpuId(uint16_t) = 0;
            virtual uint16_t GetOpuId() const = 0;
            //用户私有信息
            virtual void SetUserInfo(const char*) = 0;
            virtual const char* GetUserInfo() const = 0;
        };

        /**
         * @brief 现货订单查询消息
         */
        class QUANT_API ATPReqCashOrderQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashOrderQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashOrderQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashOrderQueryMsg(const ATPReqCashOrderQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashOrderQueryMsg& operator=(const ATPReqCashOrderQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashOrderQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashOrderQueryMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //客户订单编号，0表示查所有
            virtual void SetClOrdNo(int64_t) = 0;
            virtual int64_t GetClOrdNo() const = 0;
            //市场代码，0表示查所有。开通北交所业务后必填 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型，0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //买卖方向，'0'表示查所有
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //委托查询条件，0表示查所有
            virtual void SetOrderQueryCondition(uint8_t) = 0;
            virtual uint8_t GetOrderQueryCondition() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //返回顺序
            virtual void SetReturnSeq(uint8_t) = 0;
            virtual uint8_t GetReturnSeq() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用)(ATP3.2.3开始支持，且仅支持现货集中竞价)
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //起始查询索引号 (默认值为0，表示从头开始查），ATP4.5版本开始支持
            virtual void SetQueryIndex(int64_t) = 0;
            virtual int64_t GetQueryIndex() const = 0;
            //委托时间开始时间
            virtual void SetTransactTimeStart(int64_t) = 0;
            virtual int64_t GetTransactTimeStart() const = 0;
            //委托时间结束时间
            virtual void SetTransactTimeEnd(int64_t) = 0;
            virtual int64_t GetTransactTimeEnd() const = 0;
        };

        /**
         * @brief 现货订单查询结果
         */
        class QUANT_API ATPRspCashOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashOrderQueryResultMsg(const ATPRspCashOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashOrderQueryResultMsg& operator=(const ATPRspCashOrderQueryResultMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashOrderQueryResultMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashOrderQueryResultMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //终端识别码（ATP3.2.3 开始支持）
            virtual const char* GetClientFeatureCode() const = 0;
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //订单类别
            virtual char GetOrdType() const = 0;
            //订单标识 
            virtual uint8_t GetOrdSign() const = 0;
            //当前申报的状态（对于撤单订单，填写被撤订单的当前状态，找不到原始订单时，固定填8 = Reject ）
            virtual uint8_t GetOrdStatus() const = 0;
            //申报状态详细信息
            virtual const char* GetOrderStatusInfo() const = 0;
            //客户委托时间
            virtual int64_t GetTransactTime() const = 0;
            //委托价格N13(4)
            virtual double GetOrderPrice() const = 0;
            //平均成交价格N13(4)
            virtual double GetExecPrice() const = 0;
            //确认编号（此字段仅对深市订单有效，取值自深交所确认执行报告的ExecID;其余订单均为空）
            virtual const char* GetConfirmExecId() const = 0;
            //委托数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetOrderQty() const = 0;
            //未成交部分的数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetLeavesQty() const = 0;
            //累计成交数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetCumQty() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //交易所订单编号
            virtual const char* GetOrderId() const = 0;
            //申报合同号,上交所：以QP1开头,表示为交易所保证金强制平仓；以CV1开头,表示为交易所备兑强制平仓；
            virtual const char* GetClOrdId() const = 0;
            //对于撤单订单，为原始交易客户方（券商）订单编号，指示被撤消订单的cl_ord_no; 对于普通订单，取值为0
            virtual int64_t GetOrigClOrdNo() const = 0;
            //冻结交易金额N15(4)
            virtual double GetFrozenTradeValue() const = 0;
            //冻结费用N15(4)
            virtual double GetFrozenFee() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //委托金额N15(4)
            virtual double GetOrderEntrustedAmt() const = 0;
            //成交金额N15(4)
            virtual double GetOrderCumTransactionAmt() const = 0;
            //执行类型（ATP3.1.8 开始支持）
            virtual char GetExecType() const = 0;
            //证券类别（ATP3.1.8 开始支持）
            virtual uint16_t GetSecurityType() const = 0;
            //已撤单数量N15(2)（ATP3.1.9 开始支持）
            virtual double GetCanceledQty() const = 0;
            //订单标识（ATP3.1.9 开始支持）（返回默认值表示当前版本不支持）
            virtual char GetOrderFlag() const = 0;
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户自定义委托批号，对于撤单订单，为原订单的客户自定义委托批号。（ATP3.2.3开始支持现货集中竞价；ATP3.2.18开始支持ETF申赎）
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //报盘通道ID（ATP3.2.12.6开始支持，且仅支持现货集中竞价）
            virtual uint16_t GetOpuId() const = 0;
            //查询结果索引号 ，ATP4.5开始支持
            virtual int64_t GetIndex() const = 0;
            //交易所确认时间，当订单未被交易所确认时，返回0（ATP4.5版本开始支持）
            virtual int64_t GetExchangeTransactTime() const = 0;
        };

        /**
         * @brief 现货股份查询
         */
        class QUANT_API ATPReqCashShareQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashShareQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashShareQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashShareQueryMsg(const ATPReqCashShareQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashShareQueryMsg& operator=(const ATPReqCashShareQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashShareQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashShareQueryMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //业务类型，0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //市场代码，0表示查所有。开通北交所业务后必填
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };

        /**
         * @brief 现货股份查询结果
         */
        class QUANT_API ATPRspCashShareQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashShareQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashShareQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashShareQueryResultMsg(const ATPRspCashShareQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashShareQueryResultMsg& operator=(const ATPRspCashShareQueryResultMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashShareQueryResultMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashShareQueryResultMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //日初持仓量N15(2)
            virtual double GetInitQty() const = 0;
            //剩余股份数量N15(2)
            virtual double GetLeavesQty() const = 0;
            //可用股份数量N15(2)
            virtual double GetAvailableQty() const = 0;
            //浮动盈亏N15(4)
            virtual double GetProfitLoss() const = 0;
            //市值N15(4)
            virtual double GetMarketValue() const = 0;
            //成本价N15(4)
            virtual double GetCostPrice() const = 0;
            //最新价N13(4)
            virtual double GetLastPrice() const = 0;
            //股份买入解冻
            virtual double GetStockBuy() const = 0;
            //股份卖出冻结
            virtual double GetStockSale() const = 0;
            //证券类别
            virtual uint16_t GetSecurityType() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //对于ETF基金表示可赎回数量，对于其他证券表示可用于ETF申购的数量（ATP3.2.1开始支持）
            virtual double GetEtfRedemptionQty() const = 0;
            //当日已实现的盈亏（ATP4.0.0开始支持）
            virtual double GetRealizeProfitLoss() const = 0;
            //附加信息。当证券类型为【27-配售权证】，此字段返回配售类型：0-配股，1-配债，2-减持配售；否则为空(ATP V3.2.9开始支持)
            virtual const char* GetExtraInfo() const = 0;
        };

        /**
         * @brief 现货按证券账户划转资金请求消息
         */
        class QUANT_API ATPReqCashExternalInsTETransFundReqMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashExternalInsTETransFundReqMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashExternalInsTETransFundReqMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashExternalInsTETransFundReqMsg(const ATPReqCashExternalInsTETransFundReqMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashExternalInsTETransFundReqMsg& operator=(const ATPReqCashExternalInsTETransFundReqMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashExternalInsTETransFundReqMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashExternalInsTETransFundReqMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //资金转出账户ID 
            virtual void SetFundOutAccountId(const char*) = 0;
            virtual const char* GetFundOutAccountId() const = 0;
            //资金转出市场代码
            virtual void SetFundOutMarketId(uint16_t) = 0;
            virtual uint16_t GetFundOutMarketId() const = 0;
            //资金转入账户ID 
            virtual void SetFundInAccountId(const char*) = 0;
            virtual const char* GetFundInAccountId() const = 0;
            //资金转入市场代码
            virtual void SetFundInMarketId(uint16_t) = 0;
            virtual uint16_t GetFundInMarketId() const = 0;
            //划拨金额 N15(4)
            virtual void SetValue(double) = 0;
            virtual double GetValue() const = 0;
            //资金属性（ATP V4.0.0版本开始支持）
            virtual void SetFundProperty(char) = 0;
            virtual char GetFundProperty() const = 0;
        };

        /**
         * @brief 按证券账户划转资金响应消息
         */
        class QUANT_API ATPRspCashExternalInsTETransFundRespMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashExternalInsTETransFundRespMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashExternalInsTETransFundRespMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashExternalInsTETransFundRespMsg(const ATPRspCashExternalInsTETransFundRespMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashExternalInsTETransFundRespMsg& operator=(const ATPRspCashExternalInsTETransFundRespMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashExternalInsTETransFundRespMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashExternalInsTETransFundRespMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
        };

        /**
         * @brief 现货资金划拨请求消息
         */
        class QUANT_API ATPReqCashExtFundTransferOtherMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashExtFundTransferOtherMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashExtFundTransferOtherMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashExtFundTransferOtherMsg(const ATPReqCashExtFundTransferOtherMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashExtFundTransferOtherMsg& operator=(const ATPReqCashExtFundTransferOtherMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashExtFundTransferOtherMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashExtFundTransferOtherMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户此字段必填）
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID（ATP3.1.9开始支持非必填）
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //划拨方式 ：1=转入；2=转出
            virtual void SetTransferWay(uint8_t) = 0;
            virtual uint8_t GetTransferWay() const = 0;
            //划拨金额 N15(4) 
            virtual void SetTransferValue(double) = 0;
            virtual double GetTransferValue() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //资金属性（ATP V4.0.0版本开始支持）
            virtual void SetFundProperty(char) = 0;
            virtual char GetFundProperty() const = 0;
        };

        /**
         * @brief 现货资金划拨(转入、转出)应答消息
         */
        class QUANT_API ATPRspCashExtFundTransferResultOtherMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashExtFundTransferResultOtherMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashExtFundTransferResultOtherMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashExtFundTransferResultOtherMsg(const ATPRspCashExtFundTransferResultOtherMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashExtFundTransferResultOtherMsg& operator=(const ATPRspCashExtFundTransferResultOtherMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashExtFundTransferResultOtherMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashExtFundTransferResultOtherMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //回报时间
            virtual int64_t GetTransactTime() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //日初可用余额N15(4)
            virtual double GetInitAmt() const = 0;
            //委托预冻结N15(4)
            virtual double GetOrderFrozen() const = 0;
            //买入成交N15(4)
            virtual double GetBuyTrade() const = 0;
            //卖出成交N15(4)
            virtual double GetSellTrade() const = 0;
            //异常冻结N15(4)
            virtual double GetUnusualFrozen() const = 0;
            //异常冻结取消N15(4)
            virtual double GetUnusualFrozenCancel() const = 0;
            //冻结费用N15(4)
            virtual double GetFeeFrozen() const = 0;
            //成交费用N15(4)
            virtual double GetFeeTrade() const = 0;
            //当日入金N15(4)
            virtual double GetTodayIn() const = 0;
            //当日出金N15(4)
            virtual double GetTodayOut() const = 0;
            //临时调增N15(4)
            virtual double GetTempAdd() const = 0;
            //临时调减N15(4)
            virtual double GetTempSub() const = 0;
            //临时冻结N15(4)
            virtual double GetTempFrozen() const = 0;
            //临时冻结取消N15(4)
            virtual double GetTempFrozenCancel() const = 0;
            //调整前资金余额N15(4)
            virtual double GetPreBalance() const = 0;
            //调整前T+0可用资金N15(4)
            virtual double GetPreAvailableT0() const = 0;
            //调整前T+1在途可用资金N15(4)
            virtual double GetPreOnTheWayT1() const = 0;
            //转移金额N15(4)
            virtual double GetTransferValue() const = 0;
        };

        /**
         * @brief 现货成交查询消息
         */
        class QUANT_API ATPReqCashTradeOrderQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashTradeOrderQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashTradeOrderQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashTradeOrderQueryMsg(const ATPReqCashTradeOrderQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashTradeOrderQueryMsg& operator=(const ATPReqCashTradeOrderQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashTradeOrderQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashTradeOrderQueryMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //市场代码，0表示查所有。开通北交所业务后必填
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型， 0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //返回顺序，1表示按时间正序排序
            virtual void SetReturnSeq(uint8_t) = 0;
            virtual uint8_t GetReturnSeq() const = 0;
            //客户订单编号，0表示查所有
            virtual void SetClOrdNo(int64_t) = 0;
            virtual int64_t GetClOrdNo() const = 0;
            //报盘合同号 
            virtual void SetClOrdId(const char*) = 0;
            virtual const char* GetClOrdId() const = 0;
            //执行编号 
            virtual void SetExecId(const char*) = 0;
            virtual const char* GetExecId() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用)(ATP3.2.3开始支持，且仅支持现货集中竞价)
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //成交查询条件 （默认值为0，表示按全局参数设置的模式进行查询）
            virtual void SetTradeOrderQueryCondition(uint8_t) = 0;
            virtual uint8_t GetTradeOrderQueryCondition() const = 0;
            //起始查询索引号 (默认值为0，表示从头开始查），ATP4.5版本开始支持
            virtual void SetQueryIndex(int64_t) = 0;
            virtual int64_t GetQueryIndex() const = 0;
        };

        /**
         * @brief 现货成交查询结果
         */
        class QUANT_API ATPRspCashTradeOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashTradeOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashTradeOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashTradeOrderQueryResultMsg(const ATPRspCashTradeOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashTradeOrderQueryResultMsg& operator=(const ATPRspCashTradeOrderQueryResultMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashTradeOrderQueryResultMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashTradeOrderQueryResultMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //订单类别
            virtual char GetOrdType() const = 0;
            //执行类型（ATP3.1.8 开始支持）
            virtual char GetExecType() const = 0;
            //执行编号
            virtual const char* GetExecId() const = 0;
            //客户订单编号（若为撤单成功，为原订单编号；若为撤单失败，为撤单订单编号）
            virtual int64_t GetClOrdNo() const = 0;
            //交易所订单编号
            virtual const char* GetOrderId() const = 0;
            //申报合同号,上交所：以QP1开头,表示为交易所保证金强制平仓；以CV1开头,表示为交易所备兑强制平仓；
            virtual const char* GetClOrdId() const = 0;
            //客户委托时间
            virtual int64_t GetTransactTime() const = 0;
            //成交价格N13(4)
            virtual double GetLastPx() const = 0;
            //成交数量N15(2)
            virtual double GetLastQty() const = 0;
            //成交金额N15(4)
            virtual double GetTotalValueTraded() const = 0;
            //成交费用N15(4)
            virtual double GetFee() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //证券类别（ATP3.1.8 开始支持）
            virtual uint16_t GetSecurityType() const = 0;
            //ETF成交回报类型
            virtual uint8_t GetEtfTradeReportType() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //客户自定义委托批号
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //撤单标识：0-非撤单，1-撤单（1-撤单指示撤单失败消息，0-非撤单指示其他消息）
            virtual bool GetCancelFlag() const = 0;
            //拒绝原因代码（ATP3.5 开始支持，3.5之前固定为0，勿用）
            virtual int32_t GetRejectReasonCode() const = 0;
            //拒绝原因描述（ATP3.5 开始支持，3.5之前固定为空，勿用）
            virtual const char* GetOrdRejReason() const = 0;
            //查询结果索引号 ，ATP4.5开始支持
            virtual int64_t GetIndex() const = 0;
            //当前申报的状态 
            virtual uint8_t GetOrdStatus() const = 0;
            //成分股与资金记录总数（仅对business_type =12-ETF实时申购赎回，且基金代码数据记录有效，其他情况为0） 
            virtual int64_t GetEtfReportComponentNum() const = 0;
            //ETF消息类型
            virtual char GetEtfTradeMsgType() const = 0;
        };

        /**
         * @brief 现货证券信息查询消息
         */
        class QUANT_API ATPReqCashExtQuerySecurityInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashExtQuerySecurityInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashExtQuerySecurityInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashExtQuerySecurityInfoMsg(const ATPReqCashExtQuerySecurityInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashExtQuerySecurityInfoMsg& operator=(const ATPReqCashExtQuerySecurityInfoMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashExtQuerySecurityInfoMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashExtQuerySecurityInfoMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码，0表示查所有
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };

        /**
         * @brief 现货证券信息查询结果消息
         */
        class QUANT_API ATPRspCashExtQueryResultSecurityInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashExtQueryResultSecurityInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashExtQueryResultSecurityInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashExtQueryResultSecurityInfoMsg(const ATPRspCashExtQueryResultSecurityInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashExtQueryResultSecurityInfoMsg& operator=(const ATPRspCashExtQueryResultSecurityInfoMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //证券类别
            virtual uint16_t GetSecurityType() const = 0;
            //昨日收盘价N13(4)
            virtual double GetPrevClosePrice() const = 0;
            //涨停价N13(4)
            virtual double GetPriceUpperLimit() const = 0;
            //跌停价N13(4)
            virtual double GetPriceLowerLimit() const = 0;
            //最小变动价位N13(4)
            virtual double GetPriceTick() const = 0;
            //限价买数量下限N15(2)
            virtual double GetBuyQtyLowerLimit() const = 0;
            //限价卖数量下限N15(2)
            virtual double GetSellQtyLowerLimit() const = 0;
            //是否支持回转交易
            virtual bool GetDayTrading() const = 0;
            //涨跌幅限制
            virtual bool GetHasPriceLimit() const = 0;
            //证券状态
            virtual uint64_t GetSecurityStatus() const = 0;
            //限价买数量单位N15(2)(ATP3.3.0开始支持)
            virtual double GetBuyQtyUnit() const = 0;
            //限价卖数量单位N15(2)(ATP3.3.0开始支持)
            virtual double GetSellQtyUnit() const = 0;
            //市价买数量单位N15(2)(ATP3.3.0开始支持)
            virtual double GetMarketBuyQtyUnit() const = 0;
            //市价卖数量单位N15(2)(ATP3.3.0开始支持)
            virtual double GetMarketSellQtyUnit() const = 0;
            //市价买数量上限N15(2)(ATP3.1.10开始支持)
            virtual double GetMarketBuyQtyUpperLimit() const = 0;
            //市价买数量下限N15(2)(ATP3.1.10开始支持)
            virtual double GetMarketBuyQtyLowerLimit() const = 0;
            //市价卖数量上限N15(2)(ATP3.1.10开始支持)
            virtual double GetMarketSellQtyUpperLimit() const = 0;
            //市价卖数量下限N15(2)(ATP3.1.10开始支持)
            virtual double GetMarketSellQtyLowerLimit() const = 0;
            //限价买数量上限N15(2)(ATP3.1.10开始支持)
            virtual double GetBuyQtyUpperLimit() const = 0;
            //限价卖数量上限N15(2)(ATP3.1.10开始支持)
            virtual double GetSellQtyUpperLimit() const = 0;
            //是否尚未盈利
            virtual char GetNoProfit() const = 0;
            //是否存在表决权差异
            virtual char GetWeightedVotingRights() const = 0;
            //是否注册制
            virtual char GetIsRegistration() const = 0;
            //是否具有协议控制架构
            virtual char GetIsVie() const = 0;
        };

        /**
         * @brief 现货资金查询消息
         */
        class QUANT_API ATPReqCashFundQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashFundQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashFundQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashFundQueryMsg(const ATPReqCashFundQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashFundQueryMsg& operator=(const ATPReqCashFundQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashFundQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashFundQueryMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID （ATP3.1.7版本开始支持此字段非必填）
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID（ATP3.1.7版本开始支持此字段非必填，一户两地账户必填）
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //货币种类 （ATP3.1.7起开始支持非必填）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };

        /**
         * @brief 现货资金查询结果
         */
        class QUANT_API ATPRspCashFundQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashFundQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashFundQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashFundQueryResultMsg(const ATPRspCashFundQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashFundQueryResultMsg& operator=(const ATPRspCashFundQueryResultMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashFundQueryResultMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashFundQueryResultMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //资金余额N15(4)
            virtual double GetLeavesValue() const = 0;
            //日初资金金额N15(4)
            virtual double GetInitLeavesValue() const = 0;
            //当前所有冻结N15(4)
            virtual double GetFrozenAll() const = 0;
            //当日可取资金N15(4)
            virtual double GetAvailableT0() const = 0;
            //当日可用资金N15(4)
            virtual double GetAvailableT1() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
        };

        /**
         * @brief 现货资产查询消息
         */
        class QUANT_API ATPReqCashAssetQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashAssetQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashAssetQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashAssetQueryMsg(const ATPReqCashAssetQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashAssetQueryMsg& operator=(const ATPReqCashAssetQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashAssetQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashAssetQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商） 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //货币种类 
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };

        /**
         * @brief 现货资产查询结果
         */
        class QUANT_API ATPRspCashAssetQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashAssetQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashAssetQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashAssetQueryResultMsg(const ATPRspCashAssetQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashAssetQueryResultMsg& operator=(const ATPRspCashAssetQueryResultMsg&) = delete;
        public:
            //资金余额,N15(4)
            virtual double GetLeavesValue() const = 0;
            //日初资金金额,N15(4)
            virtual double GetInitLeavesValue() const = 0;
            //当前所有冻结,N15(4)
            virtual double GetFrozenAll() const = 0;
            //当日可取资金,N15(4)
            virtual double GetAvailableT0() const = 0;
            //当日可用资金,N15(4)
            virtual double GetAvailableT1() const = 0;
            //资产总值,N15(4)
            virtual double GetTotalAsset() const = 0;
            //总市值,N15(4)
            virtual double GetMarketValue() const = 0;
            //买入冻结,N15(4)
            virtual double GetFundBuy() const = 0;
            //卖出解冻,N15(4)
            virtual double GetFundSale() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
        };

        /**
         * @brief 现货成交汇总查询消息
         */
        class QUANT_API ATPReqCashTradeCollectQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashTradeCollectQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashTradeCollectQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashTradeCollectQueryMsg(const ATPReqCashTradeCollectQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashTradeCollectQueryMsg& operator=(const ATPReqCashTradeCollectQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashTradeCollectQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashTradeCollectQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //密码 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //市场代码，0表示查所有 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码 
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型，0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
        };

        /**
         * @brief 现货成交汇总查询结果
         */
        class QUANT_API ATPRspCashCollectQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashCollectQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashCollectQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashCollectQueryResultMsg(const ATPRspCashCollectQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashCollectQueryResultMsg& operator=(const ATPRspCashCollectQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //买入均价N13(4)
            virtual double GetBuyAvgPrice() const = 0;
            //买入累计总量（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetBuyCumQty() const = 0;
            //买入成交总金额，精确到3位小数N15(4)
            virtual double GetBuyTotalValueTraded() const = 0;
            //卖出均价N13(4)
            virtual double GetSellAvgPrice() const = 0;
            //卖出累计总量（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetSellCumQty() const = 0;
            //卖出成交金额，精确到3位小数N15(4)
            virtual double GetSellTotalValueTraded() const = 0;
            //买卖合计成交金额，精确到3位小数N15(4)
            virtual double GetTotalValueTraded() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码 
            virtual uint16_t GetMarketId() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //证券类别（ATP3.1.8 开始支持）
            virtual uint16_t GetSecurityType() const = 0;
        };

        /**
         * @brief 现货最大可委托数查询消息
         */
        class QUANT_API ATPReqCashMaxOrderQtyQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashMaxOrderQtyQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashMaxOrderQtyQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashMaxOrderQtyQueryMsg(const ATPReqCashMaxOrderQtyQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashMaxOrderQtyQueryMsg& operator=(const ATPReqCashMaxOrderQtyQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashMaxOrderQtyQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashMaxOrderQtyQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券代码 
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //密码 
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //业务类型 
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //买卖方向 
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //委托价格N13(4)(市价填0，科创版填保护限价) 
            virtual void SetPrice(double) = 0;
            virtual double GetPrice() const = 0;
            //订单类型 
            virtual void SetOrderType(char) = 0;
            virtual char GetOrderType() const = 0;
        };

        /**
         * @brief 现货最大可委托数查询结果
         */
        class QUANT_API ATPRspCashMaxOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashMaxOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashMaxOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashMaxOrderQueryResultMsg(const ATPRspCashMaxOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashMaxOrderQueryResultMsg& operator=(const ATPRspCashMaxOrderQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //最大可委托数（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetMaxOrderQty() const = 0;
            //理论可委托数（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetTheoreticalOrderQty() const = 0;
        };

        /**
         * @brief ETF信息查询消息
         */
        class QUANT_API ATPReqExtQueryETFInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqExtQueryETFInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqExtQueryETFInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqExtQueryETFInfoMsg(const ATPReqExtQueryETFInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqExtQueryETFInfoMsg& operator=(const ATPReqExtQueryETFInfoMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqExtQueryETFInfoMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqExtQueryETFInfoMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //市场代码，0表示查所有
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //申赎类型（默认值为0，表示查所有）
            virtual void SetPrType(uint8_t) = 0;
            virtual uint8_t GetPrType() const = 0;
        };

        /**
         * @brief ETF信息查询结果
         */
        class QUANT_API ATPRspExtQueryResultETFInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspExtQueryResultETFInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspExtQueryResultETFInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspExtQueryResultETFInfoMsg(const ATPRspExtQueryResultETFInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspExtQueryResultETFInfoMsg& operator=(const ATPRspExtQueryResultETFInfoMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspExtQueryResultETFInfoMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspExtQueryResultETFInfoMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //ETF代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //证券类别
            virtual uint16_t GetSecurityType() const = 0;
            //申赎类型（1-普通申赎，2-实物申赎,3-全实物申赎）
            virtual uint8_t GetPrType() const = 0;
            //是否有PD券商资格
            virtual bool GetIsPdBroker() const = 0;
            //是否允许申购
            virtual bool GetIsPurchase() const = 0;
            //是否允许赎回
            virtual bool GetIsRedemption() const = 0;
            //ETF申购赎回单位N15(2)
            virtual double GetPurchaseRedemptionUnit() const = 0;
            //ETF股票篮现金差额N15(4)
            virtual double GetEstimateCashComponent() const = 0;
            //ETF最大现金替代比例N6(5)
            virtual double GetMaxCashRatio() const = 0;
            //ETF股票篮记录数
            virtual int64_t GetTotalRecordNum() const = 0;
            //ETF单位净值
            virtual double GetNav() const = 0;
            //ETF基准单位净值
            virtual double GetNavPerCu() const = 0;
            //单账户累计申购上限N18(2)
            virtual double GetPurchaseLimitPerUser() const = 0;
            //单账户累计赎回上限N18(2)
            virtual double GetRedemptionLimitPerUser() const = 0;
            //单账户净申购上限N18(2)
            virtual double GetNetPurchaseLimitPerUser() const = 0;
            //单账户净赎回上限N18(2)
            virtual double GetNetRedemptionLimitPerUser() const = 0;
        };

        /**
         * @brief ETF成分股信息查询消息
         */
        class QUANT_API ATPReqExtQueryETFComponentInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqExtQueryETFComponentInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqExtQueryETFComponentInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqExtQueryETFComponentInfoMsg(const ATPReqExtQueryETFComponentInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqExtQueryETFComponentInfoMsg& operator=(const ATPReqExtQueryETFComponentInfoMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqExtQueryETFComponentInfoMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqExtQueryETFComponentInfoMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //成分股代码（默认值为空，表示查所有）
            virtual void SetComponentSecurityId(const char*) = 0;
            virtual const char* GetComponentSecurityId() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //申赎类型（默认值为1，表示查普通申赎）
            virtual void SetPrType(uint8_t) = 0;
            virtual uint8_t GetPrType() const = 0;
        };

        /**
         * @brief ETF成分股信息查询结果
         */
        class QUANT_API ATPRspExtQueryResultETFComponentInfoMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspExtQueryResultETFComponentInfoMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspExtQueryResultETFComponentInfoMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspExtQueryResultETFComponentInfoMsg(const ATPRspExtQueryResultETFComponentInfoMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspExtQueryResultETFComponentInfoMsg& operator=(const ATPRspExtQueryResultETFComponentInfoMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspExtQueryResultETFComponentInfoMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspExtQueryResultETFComponentInfoMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //ETF代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //成分股市场代码
            virtual uint16_t GetComponentMarketId() const = 0;
            //成分股代码
            virtual const char* GetComponentSecurityId() const = 0;
            //成分股简称
            virtual const char* GetComponentSecuritySymbol() const = 0;
            //现金替代标志
            virtual uint8_t GetSubstituteFlag() const = 0;
            //申购溢价比例N7(5)
            virtual double GetPremiumRatio() const = 0;
            //赎回折价比例N7(5)
            virtual double GetRedemptionPremiumRatio() const = 0;
            //成分股数量，最小单位度量（股，张）
            virtual double GetComponentShareQty() const = 0;
            //申购现金替代金额N15(4)
            virtual double GetPurchaseCashSubstitute() const = 0;
            //赎回现金替代金额N15(4)
            virtual double GetRedemptionCashSubstitute() const = 0;
            //申赎类型
            virtual uint8_t GetPrType() const = 0;
        };

        /**
         * @brief 客户报盘通道ID查询消息
         */
        class QUANT_API ATPReqOpuQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqOpuQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqOpuQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqOpuQueryMsg(const ATPReqOpuQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqOpuQueryMsg& operator=(const ATPReqOpuQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqOpuQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqOpuQueryMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
        };

        /**
         * @brief 客户报盘通道ID查询结果
         */
        class QUANT_API ATPRspOpuQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspOpuQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspOpuQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspOpuQueryResultMsg(const ATPRspOpuQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspOpuQueryResultMsg& operator=(const ATPRspOpuQueryResultMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspOpuQueryResultMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspOpuQueryResultMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //报盘通道ID
            virtual uint16_t GetOpuId() const = 0;
            //网关IP及端口
            virtual const char* GetServerPort() const = 0;
            //平台标识
            virtual uint16_t GetPlatformId() const = 0;
            //业务集
            virtual const char* GetBizList() const = 0;
            //网关ID
            virtual const char* GetTargetCompId() const = 0;
        };

        /**
         * @brief 一步式银证转账消息
         */
        class QUANT_API ATPReqCashBankSecuritiesFundTransferOtherMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashBankSecuritiesFundTransferOtherMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashBankSecuritiesFundTransferOtherMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashBankSecuritiesFundTransferOtherMsg(const ATPReqCashBankSecuritiesFundTransferOtherMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashBankSecuritiesFundTransferOtherMsg& operator=(const ATPReqCashBankSecuritiesFundTransferOtherMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashBankSecuritiesFundTransferOtherMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashBankSecuritiesFundTransferOtherMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //货币种类（默认值 CNY）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //划拨方式 ：1-银证转入；2-银证转出
            virtual void SetTransferWay(uint8_t) = 0;
            virtual uint8_t GetTransferWay() const = 0;
            //划拨金额 N15(4)
            virtual void SetTransferValue(double) = 0;
            virtual double GetTransferValue() const = 0;
            //客户密码（该字段是否必填请咨询券商）
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //资金密码（该字段是否必填请咨询券商）
            virtual void SetFundPassword(const char*) = 0;
            virtual const char* GetFundPassword() const = 0;
            //银行密码（该字段是否必填请咨询券商）
            virtual void SetBankPassword(const char*) = 0;
            virtual const char* GetBankPassword() const = 0;
            //备注
            virtual void SetRemark(const char*) = 0;
            virtual const char* GetRemark() const = 0;
        };

        /**
         * @brief 一步式银证转账响应消息
         */
        class QUANT_API ATPRspCashBankSecuritiesFundTransferResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashBankSecuritiesFundTransferResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashBankSecuritiesFundTransferResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashBankSecuritiesFundTransferResultMsg(const ATPRspCashBankSecuritiesFundTransferResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashBankSecuritiesFundTransferResultMsg& operator=(const ATPRspCashBankSecuritiesFundTransferResultMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashBankSecuritiesFundTransferResultMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashBankSecuritiesFundTransferResultMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //响应时间
            virtual int64_t GetTransactTime() const = 0;
            //划拨方式
            virtual uint8_t GetTransferWay() const = 0;
            //外部系统统一划拨流水号
            virtual int64_t GetTransferSerialNo() const = 0;
            //外部系统银证转账流水号
            virtual int64_t GetBankTransferSerialNo() const = 0;
            //流水号
            virtual int64_t GetSerialNo() const = 0;
            //业务状态（ATP3.2.1开始支持）
            virtual uint8_t GetBusinessStatus() const = 0;
        };

        /**
         * @brief 外部持仓划拨消息
         */
        class QUANT_API ATPReqExtShareTransferMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqExtShareTransferMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqExtShareTransferMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqExtShareTransferMsg(const ATPReqExtShareTransferMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqExtShareTransferMsg& operator=(const ATPReqExtShareTransferMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqExtShareTransferMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqExtShareTransferMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //证券代码
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //客户密码（该字段是否必填请咨询券商）
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //划拨方式 ：1=转入；2=转出
            virtual void SetTransferType(uint8_t) = 0;
            virtual uint8_t GetTransferType() const = 0;
            //划拨数量 N15(2)
            virtual void SetValue(double) = 0;
            virtual double GetValue() const = 0;
        };

        /**
         * @brief 外部持仓划拨响应消息
         */
        class QUANT_API ATPRspExtShareTransferResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspExtShareTransferResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspExtShareTransferResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspExtShareTransferResultMsg(const ATPRspExtShareTransferResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspExtShareTransferResultMsg& operator=(const ATPRspExtShareTransferResultMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspExtShareTransferResultMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspExtShareTransferResultMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //划拨数量 N15(2)
            virtual double GetValue() const = 0;
        };

        /**
         * @brief 新股中签结果查询消息
         */
        class QUANT_API ATPReqNewShareHitQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqNewShareHitQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqNewShareHitQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqNewShareHitQueryMsg(const ATPReqNewShareHitQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqNewShareHitQueryMsg& operator=(const ATPReqNewShareHitQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqNewShareHitQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqNewShareHitQueryMsg* msg_ptr);

        public:
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账号ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商）
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //申购开始日期（日期格式YYYYMMDD)，默认值为0，表示查所有
            virtual void SetStartDate(int64_t) = 0;
            virtual int64_t GetStartDate() const = 0;
            //申购结束日期（日期格式YYYYMMDD)，默认值为当前时间
            virtual void SetEndDate(int64_t) = 0;
            virtual int64_t GetEndDate() const = 0;
            //中签日期（日期格式YYYYMMDD)（是否可用请咨询券商）
            virtual void SetHitDate(int64_t) = 0;
            virtual int64_t GetHitDate() const = 0;
            //扩展字段 
            virtual void SetExtraData(const char*) = 0;
            virtual const char* GetExtraData() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };

        /**
         * @brief 新股中签结果查询结果
         */
        class QUANT_API ATPRspNewShareHitQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspNewShareHitQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspNewShareHitQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspNewShareHitQueryResultMsg(const ATPRspNewShareHitQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspNewShareHitQueryResultMsg& operator=(const ATPRspNewShareHitQueryResultMsg&) = delete;
        public:
            //客户号ID（取DXS返回内容）
            virtual const char* GetCustId() const = 0;
            //资金账号ID（取DXS返回内容）
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（取DXS返回内容）
            virtual const char* GetAccountId() const = 0;
            //扩展字段 
            virtual const char* GetExtraData() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //申购日期
            virtual int64_t GetTradeDate() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券名称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券类别（字典值请咨询券商）
            virtual const char* GetSecurityType() const = 0;
            //发行价N13(4)
            virtual double GetPrice() const = 0;
            //中签日期（日期格式YYYYMMDD)
            virtual int64_t GetHitDate() const = 0;
            //中签数量N15(2)
            virtual double GetHitQty() const = 0;
            //中签金额、应缴金额N15(4)
            virtual double GetHitValue() const = 0;
            //缴款日期（日期格式YYYYMMDD)
            virtual int64_t GetPayDate() const = 0;
            //已缴款数量N15(2)
            virtual double GetPayQty() const = 0;
            //已缴款金额N15(4)
            virtual double GetPayValue() const = 0;
            //放弃数量N15(2)
            virtual double GetGiveUpQty() const = 0;
            //交易所确认数量N15(2)
            virtual double GetLastQty() const = 0;
            //冻结金额N15(4)
            virtual double GetFrozenValue() const = 0;
        };

        /**
         * @brief 融资融券委托消息
         */
        class QUANT_API ATPReqMarginOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginOrderMsg(const ATPReqMarginOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginOrderMsg& operator=(const ATPReqMarginOrderMsg&) = delete;
            /**
             * @brief 构建消息
             * @param business_type 业务类型
             */
            static ATPReqMarginOrderMsg* NewMessage(ATPBusinessTypeType business_type);
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginOrderMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账号ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券代码 （当为配股业务时，该字段表示配售权证代码）
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //委托价格N13(4) 
            virtual void SetPrice(double) = 0;
            virtual double GetPrice() const = 0;
            //申报数量N15(2) 
            virtual void SetOrderQty(double) = 0;
            virtual double GetOrderQty() const = 0;
            //买卖方向 
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //订单类型 
            virtual void SetOrderType(char) = 0;
            virtual char GetOrderType() const = 0;
            //市场代码 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //股份性质
            virtual void SetShareNegotiability(const char*) = 0;
            virtual const char* GetShareNegotiability() const = 0;
            //对方申报交易单元
            virtual void SetDestPbuId(const char*) = 0;
            virtual const char* GetDestPbuId() const = 0;
            //对方证券账户
            virtual void SetDestAccountId(const char*) = 0;
            virtual const char* GetDestAccountId() const = 0;
            //直接还券类型（默认值255，表示不指定）
            virtual void SetRedeliverType(uint8_t) = 0;
            virtual uint8_t GetRedeliverType() const = 0;
            //指定合约偿还列表
            virtual void SetDesignatedRepayList(const char*) = 0;
            virtual const char* GetDesignatedRepayList() const = 0;
            //实时划转标志
            virtual void SetImmediateEffect(uint8_t) = 0;
            virtual uint8_t GetImmediateEffect() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用，该字段仅支持担保品集中竞价交易。)
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };

        /**
         * @brief 融资融券委托响应
         */
        class QUANT_API ATPRspMarginOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginOrderMsg(const ATPRspMarginOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginOrderMsg& operator=(const ATPRspMarginOrderMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //委托价格N13(4)
            virtual double GetPrice() const = 0;
            //委托数量N15(2)
            virtual double GetOrderQty() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //订单类型
            virtual char GetOrderType() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //响应时间
            virtual int64_t GetTransactTime() const = 0;
            //客户自定义委托批号（仅支持担保品集中竞价交易。）
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };

        /**
         * @brief 融资融券主推消息
         */
        class QUANT_API ATPRtnMarginOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRtnMarginOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRtnMarginOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRtnMarginOrderMsg(const ATPRtnMarginOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRtnMarginOrderMsg& operator=(const ATPRtnMarginOrderMsg&) = delete;
        public:
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账号ID 
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual const char* GetAccountId() const = 0;
            //证券代码 
            virtual const char* GetSecurityId() const = 0;
            //委托价格N13(4) 
            virtual double GetPrice() const = 0;
            //委托数量N15(2) 
            virtual double GetOrderQty() const = 0;
            //买卖方向 
            virtual char GetSide() const = 0;
            //市场代码 
            virtual uint16_t GetMarketId() const = 0;
            //客户订单编号 
            virtual int64_t GetClOrdNo() const = 0;
            //业务类型 
            virtual uint8_t GetBusinessType() const = 0;
            //委托时间 
            virtual int64_t GetTransactTime() const = 0;
            //客户自定义委托批号（仅支持担保品集中竞价交易。）
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //订单状态 
            virtual uint8_t GetOrdStatus() const = 0;
            //订单标识
            virtual uint8_t GetOrdSign() const = 0;
            //未成交部分的数量N15(2) 
            virtual double GetLeavesQty() const = 0;
            //累计成交数量N15(2) 
            virtual double GetCumQty() const = 0;
            //交易所订单编号 
            virtual const char* GetOrderId() const = 0;
            //申报合同号 
            virtual const char* GetClOrdId() const = 0;
            //冻结交易金额N15(4)
            virtual double GetFrozenTradeValue() const = 0;
            //冻结费用N15(4)
            virtual double GetFrozenFee() const = 0;
            //成交编号
            virtual const char* GetExecId() const = 0;
            //成交价格N13(4)
            virtual double GetLastPx() const = 0;
            //成交数量N15(2)
            virtual double GetLastQty() const = 0;
            //成交金额N15(4)
            virtual double GetTotalValueTraded() const = 0;
            //信用标识
            virtual char GetCashMargin() const = 0;
            //原委托的客户订单编号
            virtual int64_t GetOrigClOrdNo() const = 0;
            //拒绝原因代码
            virtual int32_t GetRejectReasonCode() const = 0;
            //拒绝原因描述
            virtual const char* GetOrdRejReason() const = 0;
            //成交金额（含利息）
            virtual double GetNetTotalValueTraded() const = 0;
        };

        /**
         * @brief 融资融券直接还款请求
         */
        class QUANT_API ATPReqMarginExtRepayMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExtRepayMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExtRepayMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExtRepayMsg(const ATPReqMarginExtRepayMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExtRepayMsg& operator=(const ATPReqMarginExtRepayMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExtRepayMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExtRepayMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //市场代码 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //货币种类（默认值 "CNY"）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //还款金额 N15(4) 
            virtual void SetRepayValue(double) = 0;
            virtual double GetRepayValue() const = 0;
            //还款方式 
            virtual void SetRepayWay(uint8_t) = 0;
            virtual uint8_t GetRepayWay() const = 0;
        };

        /**
         * @brief 融资融券直接还款应答消息
         */
        class QUANT_API ATPRspExtRepayResultOtherMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspExtRepayResultOtherMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspExtRepayResultOtherMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspExtRepayResultOtherMsg(const ATPRspExtRepayResultOtherMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspExtRepayResultOtherMsg& operator=(const ATPRspExtRepayResultOtherMsg&) = delete;
        public:
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual const char* GetBranchId() const = 0;
            //回报时间
            virtual int64_t GetTransactTime() const = 0;
            //还款金额N15(4)
            virtual double GetRepayAmt() const = 0;
        };

        /**
         * @brief 融资融券通用撤单消息
         */
        class QUANT_API ATPReqMarginCancelOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginCancelOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginCancelOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginCancelOrderMsg(const ATPReqMarginCancelOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginCancelOrderMsg& operator=(const ATPReqMarginCancelOrderMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginCancelOrderMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginCancelOrderMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //原委托的客户订单编号
            virtual void SetOrigClOrdNo(int64_t) = 0;
            virtual int64_t GetOrigClOrdNo() const = 0;
            //客户自定义委托批号，指示被撤消订单的batch_cl_ord_no。batch_cl_ord_no与orig_cl_ord_no至少填写一项。若orig_cl_ord_no不为0，则按orig_cl_ord_no撤单；否则，batch_cl_ord_no不为0，按batch_cl_ord_no撤单，仅支持担保品集中竞价交易
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //市场代码 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };

        /**
         * @brief 融资融券通用撤单响应
         */
        class QUANT_API ATPRspMarginCancelOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginCancelOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginCancelOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginCancelOrderMsg(const ATPRspMarginCancelOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginCancelOrderMsg& operator=(const ATPRspMarginCancelOrderMsg&) = delete;
        public:
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual const char* GetAccountId() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //原委托的客户订单编号
            virtual int64_t GetOrigClOrdNo() const = 0;
            //响应时间
            virtual int64_t GetTransactTime() const = 0;
            //客户自定义委托批号，指示被撤消订单的batch_cl_ord_no。batch_cl_ord_no与orig_cl_ord_no至少填写一项。若orig_cl_ord_no不为0，则按orig_cl_ord_no撤单；否则，batch_cl_ord_no不为0，按batch_cl_ord_no撤单，仅支持担保品集中竞价交易
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };

        /**
         * @brief 按证券账户划转资金请求消息
         */
        class QUANT_API ATPReqMarginExternalInsTETransFundReqMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExternalInsTETransFundReqMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExternalInsTETransFundReqMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExternalInsTETransFundReqMsg(const ATPReqMarginExternalInsTETransFundReqMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExternalInsTETransFundReqMsg& operator=(const ATPReqMarginExternalInsTETransFundReqMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExternalInsTETransFundReqMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExternalInsTETransFundReqMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //货币种类 （仅支持人民币）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //资金转出账户ID 
            virtual void SetFundOutAccountId(const char*) = 0;
            virtual const char* GetFundOutAccountId() const = 0;
            //资金转出市场代码 
            virtual void SetFundOutMarketId(uint16_t) = 0;
            virtual uint16_t GetFundOutMarketId() const = 0;
            //资金转入账户ID
            virtual void SetFundInAccountId(const char*) = 0;
            virtual const char* GetFundInAccountId() const = 0;
            //资金转入市场代码
            virtual void SetFundInMarketId(uint16_t) = 0;
            virtual uint16_t GetFundInMarketId() const = 0;
            //划拨金额 N15(4)
            virtual void SetValue(double) = 0;
            virtual double GetValue() const = 0;
            //资金属性（ATP V4.0.0版本开始支持）
            virtual void SetFundProperty(char) = 0;
            virtual char GetFundProperty() const = 0;
        };

        /**
         * @brief 按证券账户划转资金响应消息
         */
        class QUANT_API ATPRspMarginExternalInsTETransFundRespMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginExternalInsTETransFundRespMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginExternalInsTETransFundRespMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginExternalInsTETransFundRespMsg(const ATPRspMarginExternalInsTETransFundRespMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginExternalInsTETransFundRespMsg& operator=(const ATPRspMarginExternalInsTETransFundRespMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
        };

        /**
         * @brief 融资融券资金划拨请求消息
         */
        class QUANT_API ATPReqMarginExtFundTransferOtherMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExtFundTransferOtherMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExtFundTransferOtherMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExtFundTransferOtherMsg(const ATPReqMarginExtFundTransferOtherMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExtFundTransferOtherMsg& operator=(const ATPReqMarginExtFundTransferOtherMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExtFundTransferOtherMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExtFundTransferOtherMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户此字段必填）
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID（ATP3.1.9开始支持非必填）
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //货币种类 （仅支持人民币）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //划拨方式 ：1=转入；2=转出
            virtual void SetTransferWay(uint8_t) = 0;
            virtual uint8_t GetTransferWay() const = 0;
            //划拨金额 N15(4) 
            virtual void SetTransferValue(double) = 0;
            virtual double GetTransferValue() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };

        /**
         * @brief 融资融券资金划拨(转入、转出)应答消息
         */
        class QUANT_API ATPRspMarginExtFundTransferResultOtherMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginExtFundTransferResultOtherMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginExtFundTransferResultOtherMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginExtFundTransferResultOtherMsg(const ATPRspMarginExtFundTransferResultOtherMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginExtFundTransferResultOtherMsg& operator=(const ATPRspMarginExtFundTransferResultOtherMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //回报时间
            virtual int64_t GetTransactTime() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //日初可用余额N15(4)
            virtual double GetInitAmt() const = 0;
            //委托预冻结N15(4)
            virtual double GetOrderFrozen() const = 0;
            //买入成交N15(4)
            virtual double GetBuyTrade() const = 0;
            //卖出成交N15(4)
            virtual double GetSellTrade() const = 0;
            //异常冻结N15(4)
            virtual double GetUnusualFrozen() const = 0;
            //异常冻结取消N15(4)
            virtual double GetUnusualFrozenCancel() const = 0;
            //冻结费用N15(4)
            virtual double GetFeeFrozen() const = 0;
            //成交费用N15(4)
            virtual double GetFeeTrade() const = 0;
            //当日入金N15(4)
            virtual double GetTodayIn() const = 0;
            //当日出金N15(4)
            virtual double GetTodayOut() const = 0;
            //临时调增N15(4)
            virtual double GetTempAdd() const = 0;
            //临时调减N15(4)
            virtual double GetTempSub() const = 0;
            //临时冻结N15(4)
            virtual double GetTempFrozen() const = 0;
            //临时冻结取消N15(4)
            virtual double GetTempFrozenCancel() const = 0;
            //调整前资金余额N15(4)
            virtual double GetPreBalance() const = 0;
            //调整前T+0可用资金N15(4)
            virtual double GetPreAvailableT0() const = 0;
            //调整前T+1在途可用资金N15(4)
            virtual double GetPreOnTheWayT1() const = 0;
            //调整前T+2在途可用资金N15(4)
            virtual double GetPreOnTheWayT2() const = 0;
            //调整前T+3在途可用资金N15(4)
            virtual double GetPreOnTheWayT3() const = 0;
            //转移金额N15(4)
            virtual double GetTransferValue() const = 0;
        };

        /**
         * @brief 信用额度内部划拨消息
         */
        class QUANT_API ATPReqExternalInsTETransCreditAmountReqMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqExternalInsTETransCreditAmountReqMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqExternalInsTETransCreditAmountReqMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqExternalInsTETransCreditAmountReqMsg(const ATPReqExternalInsTETransCreditAmountReqMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqExternalInsTETransCreditAmountReqMsg& operator=(const ATPReqExternalInsTETransCreditAmountReqMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqExternalInsTETransCreditAmountReqMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqExternalInsTETransCreditAmountReqMsg* msg_ptr);

        public:
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //转出账户ID
            virtual void SetFundOutAccountId(const char*) = 0;
            virtual const char* GetFundOutAccountId() const = 0;
            //转出市场代码
            virtual void SetFundOutMarketId(uint16_t) = 0;
            virtual uint16_t GetFundOutMarketId() const = 0;
            //转入账户ID
            virtual void SetFundInAccountId(const char*) = 0;
            virtual const char* GetFundInAccountId() const = 0;
            //转入市场代码
            virtual void SetFundInMarketId(uint16_t) = 0;
            virtual uint16_t GetFundInMarketId() const = 0;
            //划拨总额度 N15(4)
            virtual void SetTotalValue(double) = 0;
            virtual double GetTotalValue() const = 0;
            //划拨融资额度 N15(4)
            virtual void SetFundValue(double) = 0;
            virtual double GetFundValue() const = 0;
            //划拨融券额度 N15(4)
            virtual void SetStocksValue(double) = 0;
            virtual double GetStocksValue() const = 0;
        };

        /**
         * @brief 信用额度内部划拨响应
         */
        class QUANT_API ATPRspExternalInsTETransCreditAmountRespMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspExternalInsTETransCreditAmountRespMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspExternalInsTETransCreditAmountRespMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspExternalInsTETransCreditAmountRespMsg(const ATPRspExternalInsTETransCreditAmountRespMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspExternalInsTETransCreditAmountRespMsg& operator=(const ATPRspExternalInsTETransCreditAmountRespMsg&) = delete;
        public:
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual const char* GetBranchId() const = 0;
        };

        /**
         * @brief 融资保底额度内部划拨消息
         */
        class QUANT_API ATPReqExternalInsTETransCrdbuyAmountReqMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqExternalInsTETransCrdbuyAmountReqMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqExternalInsTETransCrdbuyAmountReqMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqExternalInsTETransCrdbuyAmountReqMsg(const ATPReqExternalInsTETransCrdbuyAmountReqMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqExternalInsTETransCrdbuyAmountReqMsg& operator=(const ATPReqExternalInsTETransCrdbuyAmountReqMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqExternalInsTETransCrdbuyAmountReqMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqExternalInsTETransCrdbuyAmountReqMsg* msg_ptr);

        public:
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //转出账户ID
            virtual void SetFundOutAccountId(const char*) = 0;
            virtual const char* GetFundOutAccountId() const = 0;
            //转出市场代码
            virtual void SetFundOutMarketId(uint16_t) = 0;
            virtual uint16_t GetFundOutMarketId() const = 0;
            //转入账户ID
            virtual void SetFundInAccountId(const char*) = 0;
            virtual const char* GetFundInAccountId() const = 0;
            //转入市场代码
            virtual void SetFundInMarketId(uint16_t) = 0;
            virtual uint16_t GetFundInMarketId() const = 0;
            //划拨额度 N15(4)
            virtual void SetValue(double) = 0;
            virtual double GetValue() const = 0;
        };

        /**
         * @brief 融资保底额度内部划拨响应
         */
        class QUANT_API ATPRspExternalInsTETransCrdbuyAmountRespMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspExternalInsTETransCrdbuyAmountRespMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspExternalInsTETransCrdbuyAmountRespMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspExternalInsTETransCrdbuyAmountRespMsg(const ATPRspExternalInsTETransCrdbuyAmountRespMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspExternalInsTETransCrdbuyAmountRespMsg& operator=(const ATPRspExternalInsTETransCrdbuyAmountRespMsg&) = delete;
        public:
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual const char* GetBranchId() const = 0;
        };

        /**
         * @brief 融资融券订单查询消息
         */
        class QUANT_API ATPReqMarginOrderQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginOrderQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginOrderQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginOrderQueryMsg(const ATPReqMarginOrderQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginOrderQueryMsg& operator=(const ATPReqMarginOrderQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginOrderQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginOrderQueryMsg* msg_ptr);

        public:
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID（一户两地账户此字段必填）
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //客户订单编号，0表示查所有
            virtual void SetClOrdNo(int64_t) = 0;
            virtual int64_t GetClOrdNo() const = 0;
            //市场代码，0表示查所有。开通北交所业务后必填 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型，0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //买卖方向，'0'表示查所有
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //委托查询条件，0表示查所有
            virtual void SetOrderQueryCondition(uint8_t) = 0;
            virtual uint8_t GetOrderQueryCondition() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //返回顺序
            virtual void SetReturnSeq(uint8_t) = 0;
            virtual uint8_t GetReturnSeq() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用)(ATP3.2.3开始支持，且仅支持现货集中竞价)
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };

        /**
         * @brief 融资融券订单查询结果
         */
        class QUANT_API ATPRspMarginOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginOrderQueryResultMsg(const ATPRspMarginOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginOrderQueryResultMsg& operator=(const ATPRspMarginOrderQueryResultMsg&) = delete;
        public:
            //终端识别码（ATP3.2.3 开始支持）
            virtual const char* GetClientFeatureCode() const = 0;
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //订单类别
            virtual char GetOrdType() const = 0;
            //订单标识 
            virtual uint8_t GetOrdSign() const = 0;
            //当前申报的状态（对于撤单订单，填写被撤订单的当前状态，找不到原始订单时，固定填8 = Reject ）
            virtual uint8_t GetOrdStatus() const = 0;
            //申报状态详细信息
            virtual const char* GetOrderStatusInfo() const = 0;
            //客户委托时间
            virtual int64_t GetTransactTime() const = 0;
            //委托价格N13(4)
            virtual double GetOrderPrice() const = 0;
            //平均成交价格N13(4)
            virtual double GetExecPrice() const = 0;
            //委托数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetOrderQty() const = 0;
            //未成交部分的数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetLeavesQty() const = 0;
            //累计成交数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetCumQty() const = 0;
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //交易所订单编号
            virtual const char* GetOrderId() const = 0;
            //申报合同号,上交所：以QP1开头,表示为交易所保证金强制平仓；以CV1开头,表示为交易所备兑强制平仓；
            virtual const char* GetClOrdId() const = 0;
            //对于撤单订单，为原始交易客户方（券商）订单编号，指示被撤消订单的cl_ord_no; 对于普通订单，取值为0
            virtual int64_t GetOrigClOrdNo() const = 0;
            //冻结交易金额N15(4)
            virtual double GetFrozenTradeValue() const = 0;
            //冻结费用N15(4)
            virtual double GetFrozenFee() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //委托金额N15(4)
            virtual double GetOrderEntrustedAmt() const = 0;
            //成交金额N15(4)
            virtual double GetOrderCumTransactionAmt() const = 0;
            //执行类型（ATP3.1.8 开始支持）
            virtual char GetExecType() const = 0;
            //证券类别（ATP3.1.8 开始支持）
            virtual uint16_t GetSecurityType() const = 0;
            //已撤单数量N15(2)（ATP3.1.9 开始支持）
            virtual double GetCanceledQty() const = 0;
            //订单标识（ATP3.1.9 开始支持）（返回默认值表示当前版本不支持）
            virtual char GetOrderFlag() const = 0;
            //客户号ID 
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户自定义委托批号，对于撤单订单，为原订单的客户自定义委托批号，仅支持担保品集中竞价交易
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };

        /**
         * @brief 融资融券成交查询消息
         */
        class QUANT_API ATPReqMarginTradeOrderQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginTradeOrderQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginTradeOrderQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginTradeOrderQueryMsg(const ATPReqMarginTradeOrderQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginTradeOrderQueryMsg& operator=(const ATPReqMarginTradeOrderQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginTradeOrderQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginTradeOrderQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码，0表示查所有。开通北交所业务后必填
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型， 0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //返回顺序，1表示按时间正序排序
            virtual void SetReturnSeq(uint8_t) = 0;
            virtual uint8_t GetReturnSeq() const = 0;
            //客户订单编号，0表示查所有
            virtual void SetClOrdNo(int64_t) = 0;
            virtual int64_t GetClOrdNo() const = 0;
            //报盘合同号 
            virtual void SetClOrdId(const char*) = 0;
            virtual const char* GetClOrdId() const = 0;
            //执行编号 
            virtual void SetExecId(const char*) = 0;
            virtual const char* GetExecId() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用)(ATP3.2.3开始支持，且仅支持现货集中竞价)
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //成交查询条件 （默认值为0，表示按全局参数设置的模式进行查询）
            virtual void SetTradeOrderQueryCondition(uint8_t) = 0;
            virtual uint8_t GetTradeOrderQueryCondition() const = 0;
        };

        /**
         * @brief 融资融券成交查询结果
         */
        class QUANT_API ATPRspMarginTradeOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginTradeOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginTradeOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginTradeOrderQueryResultMsg(const ATPRspMarginTradeOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginTradeOrderQueryResultMsg& operator=(const ATPRspMarginTradeOrderQueryResultMsg&) = delete;
        public:
            //业务类型
            virtual uint8_t GetBusinessType() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //订单类别
            virtual char GetOrdType() const = 0;
            //执行类型（ATP3.1.8 开始支持）
            virtual char GetExecType() const = 0;
            //执行编号
            virtual const char* GetExecId() const = 0;
            //客户订单编号（若为撤单成功，为原订单编号；若为撤单失败，为撤单订单编号）
            virtual int64_t GetClOrdNo() const = 0;
            //交易所订单编号
            virtual const char* GetOrderId() const = 0;
            //申报合同号,上交所：以QP1开头,表示为交易所保证金强制平仓；以CV1开头,表示为交易所备兑强制平仓；
            virtual const char* GetClOrdId() const = 0;
            //客户委托时间
            virtual int64_t GetTransactTime() const = 0;
            //成交价格N13(4)
            virtual double GetLastPx() const = 0;
            //成交数量N15(2)
            virtual double GetLastQty() const = 0;
            //成交金额N15(4)
            virtual double GetTotalValueTraded() const = 0;
            //成交费用N15(4)
            virtual double GetFee() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //证券类别（ATP3.1.8 开始支持）
            virtual uint16_t GetSecurityType() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //客户自定义委托批号
            virtual uint64_t GetBatchClOrdNo() const = 0;
        };

        /**
         * @brief 融资融券股份查询
         */
        class QUANT_API ATPReqMarginShareQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginShareQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginShareQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginShareQueryMsg(const ATPReqMarginShareQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginShareQueryMsg& operator=(const ATPReqMarginShareQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginShareQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginShareQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID（一户两地账户必填）
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //业务类型，0表示查所有
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //市场代码，0表示查所有
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };

        /**
         * @brief 融资融券股份查询结果
         */
        class QUANT_API ATPRspMarginShareQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginShareQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginShareQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginShareQueryResultMsg(const ATPRspMarginShareQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginShareQueryResultMsg& operator=(const ATPRspMarginShareQueryResultMsg&) = delete;
        public:
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //日初持仓量N15(2)
            virtual double GetInitQty() const = 0;
            //剩余股份数量N15(2)
            virtual double GetLeavesQty() const = 0;
            //可用股份数量N15(2)
            virtual double GetAvailableQty() const = 0;
            //浮动盈亏N15(4)
            virtual double GetProfitLoss() const = 0;
            //市值N15(4)
            virtual double GetMarketValue() const = 0;
            //成本价N15(4)
            virtual double GetCostPrice() const = 0;
            //最新价N13(4)
            virtual double GetLastPrice() const = 0;
            //股份买入解冻
            virtual double GetStockBuy() const = 0;
            //股份卖出冻结
            virtual double GetStockSale() const = 0;
            //证券类别
            virtual uint16_t GetSecurityType() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //当日已实现的盈亏（ATP4.0.0开始支持）
            virtual double GetRealizeProfitLoss() const = 0;
            //附加信息。当证券类型为【27-配售权证】，此字段返回配售类型：0-配股，1-配债，2-减持配售；否则为空(ATP V3.2.9开始支持)
            virtual const char* GetExtraInfo() const = 0;
        };

        /**
         * @brief 信用账户可融资金查询
         */
        class QUANT_API ATPReqMarginTradingAvailableAmtQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginTradingAvailableAmtQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginTradingAvailableAmtQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginTradingAvailableAmtQueryMsg(const ATPReqMarginTradingAvailableAmtQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginTradingAvailableAmtQueryMsg& operator=(const ATPReqMarginTradingAvailableAmtQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginTradingAvailableAmtQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginTradingAvailableAmtQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //货币种类 （仅支持人民币）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
        };

        /**
         * @brief 信用账户可融资金查询结果
         */
        class QUANT_API ATPRspMarginTradingPrivateAmoutQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginTradingPrivateAmoutQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginTradingPrivateAmoutQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginTradingPrivateAmoutQueryResultMsg(const ATPRspMarginTradingPrivateAmoutQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginTradingPrivateAmoutQueryResultMsg& operator=(const ATPRspMarginTradingPrivateAmoutQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //保底额度N15(4)
            virtual double GetPrivateAmount() const = 0;
            //保底额度占用N15(4)
            virtual double GetPrivateAmountOccupied() const = 0;
            //公用额度占用N15(4)
            virtual double GetPublicAmountOccupied() const = 0;
            //是否先用保底额度
            virtual uint8_t GetFirstUsePrivateAmount() const = 0;
            //保底额度可用N15(4)
            virtual double GetPrivateAvailable() const = 0;
            //公用额度可用N15(4)
            virtual double GetPublicAvailable() const = 0;
        };

        /**
         * @brief 融资融券资金查询消息
         */
        class QUANT_API ATPReqMarginFundQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginFundQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginFundQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginFundQueryMsg(const ATPReqMarginFundQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginFundQueryMsg& operator=(const ATPReqMarginFundQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginFundQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginFundQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID （ATP3.1.7版本开始支持此字段非必填）
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID（ATP3.1.7版本开始支持此字段非必填，一户两地账户必填）
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //货币种类 （ATP3.1.7起开始支持非必填）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };

        /**
         * @brief 融资融券资金查询结果
         */
        class QUANT_API ATPRspMarginFundQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginFundQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginFundQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginFundQueryResultMsg(const ATPRspMarginFundQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginFundQueryResultMsg& operator=(const ATPRspMarginFundQueryResultMsg&) = delete;
        public:
            //资金余额N15(4)
            virtual double GetLeavesValue() const = 0;
            //日初资金金额N15(4)
            virtual double GetInitLeavesValue() const = 0;
            //当前所有冻结N15(4)
            virtual double GetFrozenAll() const = 0;
            //当日可取资金N15(4)
            virtual double GetAvailableT0() const = 0;
            //当日可用资金N15(4)
            virtual double GetAvailableT1() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //资金账户ID 
            virtual const char* GetFundAccountId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //融券卖出冻结金额N15(4)、买高流通股票、买券还券时可全额使用
            virtual double GetCreditSellFrozen() const = 0;
            //融券卖出款占用N15(4)
            virtual double GetCreditSellOccupied() const = 0;
            //融券卖出款预占用N15(4)
            virtual double GetCreditSellPreOccupied() const = 0;
            //日初融券卖出款N15(4)
            virtual double GetInitCreditSell() const = 0;
        };

        /**
         * @brief 融资融券最大可委托数查询消息
         */
        class QUANT_API ATPReqMarginMaxOrderQtyQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginMaxOrderQtyQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginMaxOrderQtyQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginMaxOrderQtyQueryMsg(const ATPReqMarginMaxOrderQtyQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginMaxOrderQtyQueryMsg& operator=(const ATPReqMarginMaxOrderQtyQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginMaxOrderQtyQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginMaxOrderQtyQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码 
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //营业部ID （ATP3.1.7版本开始支持此字段非必填）
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券代码 （当为配股业务时，该字段表示配售权证代码）
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //业务类型 
            virtual void SetBusinessType(uint8_t) = 0;
            virtual uint8_t GetBusinessType() const = 0;
            //买卖方向 
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //委托价格N13(4) （上海市场股票、存托凭证、基金市价委托必须输入保护价） 
            virtual void SetPrice(double) = 0;
            virtual double GetPrice() const = 0;
            //订单类型 
            virtual void SetOrderType(char) = 0;
            virtual char GetOrderType() const = 0;
            //直接还券类型（默认值255，表示不指定）
            virtual void SetRedeliverType(uint8_t) = 0;
            virtual uint8_t GetRedeliverType() const = 0;
        };

        /**
         * @brief 融资融券最大可委托数查询结果
         */
        class QUANT_API ATPRspMarginMaxOrderQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginMaxOrderQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginMaxOrderQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginMaxOrderQueryResultMsg(const ATPRspMarginMaxOrderQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginMaxOrderQueryResultMsg& operator=(const ATPRspMarginMaxOrderQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //最大可委托数（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetMaxOrderQty() const = 0;
            //理论可委托数（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）N15(2)
            virtual double GetTheoreticalOrderQty() const = 0;
        };

        /**
         * @brief 信用账户可融数量查询消息
         */
        class QUANT_API ATPReqMarginTradingAvailableQtyQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginTradingAvailableQtyQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginTradingAvailableQtyQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginTradingAvailableQtyQueryMsg(const ATPReqMarginTradingAvailableQtyQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginTradingAvailableQtyQueryMsg& operator=(const ATPReqMarginTradingAvailableQtyQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginTradingAvailableQtyQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginTradingAvailableQtyQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };

        /**
         * @brief 信用账户可融数量查询结果
         */
        class QUANT_API ATPRspMarginQuerySecuritiesLendingPrivateAmountQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginQuerySecuritiesLendingPrivateAmountQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginQuerySecuritiesLendingPrivateAmountQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginQuerySecuritiesLendingPrivateAmountQueryResultMsg(const ATPRspMarginQuerySecuritiesLendingPrivateAmountQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginQuerySecuritiesLendingPrivateAmountQueryResultMsg& operator=(const ATPRspMarginQuerySecuritiesLendingPrivateAmountQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //保底额度N15(2)
            virtual double GetPrivateAmount() const = 0;
            //保底额度占用N15(2)
            virtual double GetPrivateAmountOccupied() const = 0;
            //公用额度占用N15(2)
            virtual double GetPublicAmountOccupied() const = 0;
            //是否先用保底额度
            virtual uint8_t GetFirstUsePrivateAmount() const = 0;
            //转融券到期数量
            virtual double GetRefEndQty() const = 0;
            //保底额度可用N15(2)
            virtual double GetPrivateAvailable() const = 0;
            //公用额度可用N15(2)
            virtual double GetPublicAvailable() const = 0;
            //当日归还数量
            virtual double GetTodayRepayQty() const = 0;
        };

        /**
         * @brief 融资融券保证金可用查询消息
         */
        class QUANT_API ATPReqMarginExtQueryMarginUseableMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExtQueryMarginUseableMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExtQueryMarginUseableMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExtQueryMarginUseableMsg(const ATPReqMarginExtQueryMarginUseableMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExtQueryMarginUseableMsg& operator=(const ATPReqMarginExtQueryMarginUseableMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExtQueryMarginUseableMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExtQueryMarginUseableMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //查询内容 '0'= 只查询余额，'1'= 查询余额及明细
            virtual void SetQueryContents(char) = 0;
            virtual char GetQueryContents() const = 0;
            //市场代码 （默认值为0，表示查所有，一户两地账户表示查单个证券账户下所有）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码 （默认值为空字符串，表示查所有，一户两地账户表示查单个证券账户下所有）
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
        };

        /**
         * @brief 融资融券保证金可用查询结果
         */
        class QUANT_API ATPRspMarginQueryMarginUseableQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginQueryMarginUseableQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginQueryMarginUseableQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginQueryMarginUseableQueryResultMsg(const ATPRspMarginQueryMarginUseableQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginQueryMarginUseableQueryResultMsg& operator=(const ATPRspMarginQueryMarginUseableQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //保证金可用余额N15(4)
            virtual double GetMarginUseableBalance() const = 0;
        };

        /**
         * @brief 融资融券资金负债查询消息
         */
        class QUANT_API ATPReqMarginExtQueryFundLiabilityMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExtQueryFundLiabilityMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExtQueryFundLiabilityMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExtQueryFundLiabilityMsg(const ATPReqMarginExtQueryFundLiabilityMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExtQueryFundLiabilityMsg& operator=(const ATPReqMarginExtQueryFundLiabilityMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExtQueryFundLiabilityMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExtQueryFundLiabilityMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //货币种类 （仅支持人民币）
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
        };

        /**
         * @brief 融资融券资金负债查询结果
         */
        class QUANT_API ATPRspMarginQueryFundLiabilityQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginQueryFundLiabilityQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginQueryFundLiabilityQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginQueryFundLiabilityQueryResultMsg(const ATPRspMarginQueryFundLiabilityQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginQueryFundLiabilityQueryResultMsg& operator=(const ATPRspMarginQueryFundLiabilityQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //实时余额N15(4)
            virtual double GetRealTimeBalance() const = 0;
            //期初余额N15(4)
            virtual double GetInitBalance() const = 0;
            //已结未付利息N15(4)
            virtual double GetSettleUnpayInterest() const = 0;
            //已结未付罚息N15(4)
            virtual double GetSettleUnpayPenalty() const = 0;
            //未结利息N15(4)
            virtual double GetUnpayInterest() const = 0;
            //未结罚息N15(4)
            virtual double GetUnpayPenalty() const = 0;
            //当日开仓N15(4)
            virtual double GetTodayOpen() const = 0;
            //当日偿还N15(4)
            virtual double GetTodayRepay() const = 0;
            //融资保证金占用N15(4)
            virtual double GetCrdBuyRequired() const = 0;
            //融券保证金占用N15(4)
            virtual double GetCrdSellRequired() const = 0;
            //融资未成交金额N15(4)
            virtual double GetCrdBuyUnTrade() const = 0;
            //融券未成交金额N15(4)
            virtual double GetCrdSellUnTrade() const = 0;
            //临时调增N15(4)
            virtual double GetTempAdd() const = 0;
            //临时调减N15(4)
            virtual double GetTempSub() const = 0;
            //所属分区号
            virtual uint8_t GetTePartitionNo() const = 0;
            //保底额度占用N15(4)
            virtual double GetPrivateAmountOccupied() const = 0;
            //公用额度占用N15(4)
            virtual double GetPublicAmountOccupied() const = 0;
            //保底额度冻结N15(4)
            virtual double GetPrivateAmountFrozen() const = 0;
            //公用额度冻结N15(4)
            virtual double GetPublicAmountFrozen() const = 0;
            //当日指定偿还额，ATP3.1.11开始支持
            virtual double GetTodayDesignateRepayAmt() const = 0;
        };

        /**
         * @brief 融资融券合约明细查询消息
         */
        class QUANT_API ATPReqMarginExtQueryContractSpecificationsMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExtQueryContractSpecificationsMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExtQueryContractSpecificationsMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExtQueryContractSpecificationsMsg(const ATPReqMarginExtQueryContractSpecificationsMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExtQueryContractSpecificationsMsg& operator=(const ATPReqMarginExtQueryContractSpecificationsMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExtQueryContractSpecificationsMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExtQueryContractSpecificationsMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //了结标识 （默认值为'0'，表示查所有；'1'-未了结包含、'3'-全数未了结、'4'-部分未了结的内容；一户两地账户表示查单个证券账户下所有）
            virtual void SetCloseOutFlag(char) = 0;
            virtual char GetCloseOutFlag() const = 0;
            //信用标识集合 （默认值为空字符串，表示查所有；标志位之间请用逗号隔开，如需查询融券合约与融资利息两种合约明细请填写"2,3" 
            virtual void SetCashMarginSet(const char*) = 0;
            virtual const char* GetCashMarginSet() const = 0;
            //开仓日期 （默认值为0，格式为YYYYMMDD，如20210514）
            virtual void SetOpeningDate(int64_t) = 0;
            virtual int64_t GetOpeningDate() const = 0;
            //开仓流水号 （默认值为空字符串）
            virtual void SetOpeningSerialNum(const char*) = 0;
            virtual const char* GetOpeningSerialNum() const = 0;
            //查询返回数量，0表示查所有
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
            //返回顺序，1表示按时间正序排序
            virtual void SetReturnSeq(uint8_t) = 0;
            virtual uint8_t GetReturnSeq() const = 0;
            //合同序号（ATP4.0.0版本开始支持）
            virtual void SetContractSerialNum(const char*) = 0;
            virtual const char* GetContractSerialNum() const = 0;
            //证券代码，空字符串表示查所有
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
        };

        /**
         * @brief 融资融券合约明细查询结果
         */
        class QUANT_API ATPRspMarginQueryContractSpecificationsQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginQueryContractSpecificationsQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginQueryContractSpecificationsQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginQueryContractSpecificationsQueryResultMsg(const ATPRspMarginQueryContractSpecificationsQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginQueryContractSpecificationsQueryResultMsg& operator=(const ATPRspMarginQueryContractSpecificationsQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //开仓日期
            virtual int64_t GetOpeningDate() const = 0;
            //开仓流水号
            virtual const char* GetOpeningSerialNum() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //融资融券合约信用标识
            virtual uint8_t GetCreditIdentification() const = 0;
            //开仓金额N15(4)
            virtual double GetOpeningAmt() const = 0;
            //成交金额N15(4)
            virtual double GetTradingAmt() const = 0;
            //开仓数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetOpeningQty() const = 0;
            //开仓价格N13(4)
            virtual double GetOpeningPrice() const = 0;
            //已偿还金额N15(4)
            virtual double GetRepaymentAmt() const = 0;
            //已偿还数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetRepaymentQty() const = 0;
            //到期日
            virtual int64_t GetExpirationDate() const = 0;
            //利率产品
            virtual uint8_t GetInterestRateProduct() const = 0;
            //已展期次数
            virtual uint32_t GetRollOverNumber() const = 0;
            //保证金比例N3(4)
            virtual uint64_t GetDepositRation() const = 0;
            //关联流水号
            virtual const char* GetAssociateSerialNum() const = 0;
            //关联日期
            virtual int64_t GetAssociateDate() const = 0;
            //未结利息N15(4)
            virtual double GetUnsettledInterest() const = 0;
            //未结罚息N15(4)
            virtual double GetUnsettledDefaultInterest() const = 0;
            //是否先用保底额度
            virtual uint8_t GetFirstUsePrivateAmount() const = 0;
            //合同序号（ATP4.0.0版本开始支持）
            virtual const char* GetContractSerialNum() const = 0;
            //当日偿还数量
            virtual double GetTodayRepay() const = 0;
            //已占用保证金
            virtual double GetCrdRequired() const = 0;
            //了结标识
            virtual char GetCloseOutFlag() const = 0;
        };

        /**
         * @brief 融资融券信用额度查询消息
         */
        class QUANT_API ATPReqMarginExtQueryCreditAmountMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExtQueryCreditAmountMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExtQueryCreditAmountMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExtQueryCreditAmountMsg(const ATPReqMarginExtQueryCreditAmountMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExtQueryCreditAmountMsg& operator=(const ATPReqMarginExtQueryCreditAmountMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExtQueryCreditAmountMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExtQueryCreditAmountMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
        };

        /**
         * @brief 融资融券信用额度查询结果
         */
        class QUANT_API ATPRspMarginQueryCreditAmountQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginQueryCreditAmountQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginQueryCreditAmountQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginQueryCreditAmountQueryResultMsg(const ATPRspMarginQueryCreditAmountQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginQueryCreditAmountQueryResultMsg& operator=(const ATPRspMarginQueryCreditAmountQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //授信总额度N15(4)
            virtual double GetFinancingSecurityAmount() const = 0;
            //融资额度N15(4)
            virtual double GetFinancingAmount() const = 0;
            //融券额度N15(4)
            virtual double GetSecurityAmount() const = 0;
            //融资占用额度N15(4)
            virtual double GetFinancingRequiredAmount() const = 0;
            //融券占用额度N15(4)
            virtual double GetSecurityRequiredAmount() const = 0;
        };

        /**
         * @brief 融资融券股份负债查询消息
         */
        class QUANT_API ATPReqMarginExtQueryShareLiabilityMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExtQueryShareLiabilityMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExtQueryShareLiabilityMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExtQueryShareLiabilityMsg(const ATPReqMarginExtQueryShareLiabilityMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExtQueryShareLiabilityMsg& operator=(const ATPReqMarginExtQueryShareLiabilityMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExtQueryShareLiabilityMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExtQueryShareLiabilityMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填）
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //查询返回数量
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };

        /**
         * @brief 融资融券股份负债查询结果
         */
        class QUANT_API ATPRspMarginQueryShareLiabilityQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginQueryShareLiabilityQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginQueryShareLiabilityQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginQueryShareLiabilityQueryResultMsg(const ATPRspMarginQueryShareLiabilityQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginQueryShareLiabilityQueryResultMsg& operator=(const ATPRspMarginQueryShareLiabilityQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //实时数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetRealTimeQty() const = 0;
            //市值N15(4)
            virtual double GetMarketValue() const = 0;
            //期初数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetInitQty() const = 0;
            //当日开仓N15(2)
            virtual double GetTodayOpen() const = 0;
            //当日偿还N15(2)
            virtual double GetTodayRepay() const = 0;
            //当日余券N15(2)
            virtual double GetTodayLeaves() const = 0;
            //保证金占用N15(4)
            virtual double GetMarginRequired() const = 0;
            //临时调增N15(2)
            virtual double GetTempAdd() const = 0;
            //临时调减N15(2)
            virtual double GetTempSub() const = 0;
            //当日指定偿还数量，ATP3.1.11开始支持
            virtual double GetTodayDesignateRepayQty() const = 0;
        };

        /**
         * @brief 融资融券标的清单查询消息
         */
        class QUANT_API ATPReqMarginExtQueryMarginUnderlyingMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqMarginExtQueryMarginUnderlyingMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqMarginExtQueryMarginUnderlyingMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqMarginExtQueryMarginUnderlyingMsg(const ATPReqMarginExtQueryMarginUnderlyingMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqMarginExtQueryMarginUnderlyingMsg& operator=(const ATPReqMarginExtQueryMarginUnderlyingMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqMarginExtQueryMarginUnderlyingMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqMarginExtQueryMarginUnderlyingMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID 
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID（一户两地账户必填） 
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //营业部ID 
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //市场代码 （默认值为0，表示查所有，一户两地账户表示查单个证券账户下所有）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码 （默认值为空字符串，表示查所有，一户两地账户表示查单个证券账户下所有）
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //标的类型 （默认值为0，表示查所有，一户两地账户表示查单个证券账户下所有）
            virtual void SetUnderlyingQueryType(uint16_t) = 0;
            virtual uint16_t GetUnderlyingQueryType() const = 0;
            //是否输出融券专户可用数量（默认值为0，表示不输出：记录都输出且融券专户可用数量默认0）
            virtual void SetIsQueryMarginQty(uint8_t) = 0;
            virtual uint8_t GetIsQueryMarginQty() const = 0;
            //查询返回数量 （默认值为0，表示按能返回的最大数量返回，具体数量请咨询券商）
            virtual void SetReturnNum(int64_t) = 0;
            virtual int64_t GetReturnNum() const = 0;
        };

        /**
         * @brief 融资融券标的清单查询结果
         */
        class QUANT_API ATPRspMarginUnderlyingQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspMarginUnderlyingQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspMarginUnderlyingQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspMarginUnderlyingQueryResultMsg(const ATPRspMarginUnderlyingQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspMarginUnderlyingQueryResultMsg& operator=(const ATPRspMarginUnderlyingQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //标的类型（多个权限之间逗号分隔：1-担保品；2-融券；3-融资）
            virtual const char* GetUnderlyingType() const = 0;
            //折算率N13(4)
            virtual int64_t GetTranslationProportion() const = 0;
            //业务开关（多个开关之间逗号分隔：1-暂停提交；2-允许展期；3-集中度控制；4-高流通性证券）
            virtual const char* GetBusinessSwitch() const = 0;
            //融资保证金比率N3(4)（例：508表示50.8%）
            virtual uint64_t GetFinancingGuaranteeRatio() const = 0;
            //融券保证金比率N3(4)（例：508表示50.8%）
            virtual uint64_t GetSecurityGuaranteeRatio() const = 0;
        };

        /**
         * @brief 现货流量查询消息
         */
        class QUANT_API ATPReqCashOrderFlowQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashOrderFlowQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashOrderFlowQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashOrderFlowQueryMsg(const ATPReqCashOrderFlowQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashOrderFlowQueryMsg& operator=(const ATPReqCashOrderFlowQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashOrderFlowQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashOrderFlowQueryMsg* msg_ptr);

        public:
            //客户号ID 
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //证券账户ID
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //客户密码（该字段是否必填请咨询券商）
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
        };

        /**
         * @brief 现货流量查询结果消息
         */
        class QUANT_API ATPRspCashOrderFlowQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashOrderFlowQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashOrderFlowQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashOrderFlowQueryResultMsg(const ATPRspCashOrderFlowQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashOrderFlowQueryResultMsg& operator=(const ATPRspCashOrderFlowQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //当前委撤笔数N13(4)
            virtual double GetOrderTotalCount() const = 0;
            //风控指标
            virtual uint16_t GetRiskControlIndicator() const = 0;
            //计算维度
            virtual uint8_t GetCalculateDimension() const = 0;
            //预警阈值N13(4)，未设置时返回0
            virtual double GetWarningValue() const = 0;
            //预警阈值N13(4)，未设置时返回0
            virtual double GetLimitValue() const = 0;
            //超禁止阈值后阻断范围,右起第一个bit位为委托、第二个bit位为撤单，值为1表示阻断。如值为1表示禁止委托，值为3表示禁止委托和禁止撤单。
            virtual uint16_t GetLimitOperation() const = 0;
        };

        /**
         * @brief 现货风控预警消息
         */
        class QUANT_API ATPRtnCashRiskControlWarningMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRtnCashRiskControlWarningMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRtnCashRiskControlWarningMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRtnCashRiskControlWarningMsg(const ATPRtnCashRiskControlWarningMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRtnCashRiskControlWarningMsg& operator=(const ATPRtnCashRiskControlWarningMsg&) = delete;
        public:
            //客户订单编号
            virtual int64_t GetClOrdNo() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //风控指标
            virtual uint16_t GetRiskControlIndicator() const = 0;
            //计算维度
            virtual uint8_t GetCalculateDimension() const = 0;
            //系统设置的预警值N13(4)
            virtual double GetWarningValue() const = 0;
            //风控指标计算结果值N13(4)
            virtual double GetIndicatorValue() const = 0;
        };

        /**
         * @brief ETF成分股客户持仓信息查询消息
         */
        class QUANT_API ATPReqExtETFComponentPositionQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqExtETFComponentPositionQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqExtETFComponentPositionQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqExtETFComponentPositionQueryMsg(const ATPReqExtETFComponentPositionQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqExtETFComponentPositionQueryMsg& operator=(const ATPReqExtETFComponentPositionQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqExtETFComponentPositionQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqExtETFComponentPositionQueryMsg* msg_ptr);

        public:
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //买卖方向（D-场内申购，P-场外申购）（默认值为D，表示查场内申赎）
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //申购基金份额N15(2)
            virtual void SetOrderQty(double) = 0;
            virtual double GetOrderQty() const = 0;
            //联动市场。对支持全实物申赎的ETF有效，上海跨市场股票ETF的联动市场为深圳，深圳跨市场股票ETF的联动市场为上海
            virtual void SetRelatedMarketId(uint16_t) = 0;
            virtual uint16_t GetRelatedMarketId() const = 0;
            //联动市场证券账户。对支持全实物申赎的ETF有效，同资金账户下联动市场存在多个证券账户时，联动市场和联动市场证券账户必填。
            virtual void SetRelatedAccountId(const char*) = 0;
            virtual const char* GetRelatedAccountId() const = 0;
            //是否仅返回存在缺口的成分股信息（0-否，1-是）。默认1-是
            virtual void SetQtyGapFlag(uint8_t) = 0;
            virtual uint8_t GetQtyGapFlag() const = 0;
            //客户密码（该字段是否必填请咨询券商）
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
        };

        /**
         * @brief ETF成分股客户持仓信息查询结果消息
         */
        class QUANT_API ATPRspExtETFComponentPositionQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspExtETFComponentPositionQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspExtETFComponentPositionQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspExtETFComponentPositionQueryResultMsg(const ATPRspExtETFComponentPositionQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspExtETFComponentPositionQueryResultMsg& operator=(const ATPRspExtETFComponentPositionQueryResultMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //证券简称
            virtual const char* GetSecuritySymbol() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //申赎类型（1-普通申赎，2-实物申赎,3-全实物申赎）
            virtual uint8_t GetPrType() const = 0;
            //是否有PD券商资格
            virtual bool GetIsPdBroker() const = 0;
            //ETF申购赎回单位N15(2)
            virtual double GetPurchaseRedemptionUnit() const = 0;
            //ETF最大现金替代比例N6(5)
            virtual double GetMaxCashRatio() const = 0;
            //当前申购所需资金 N13(4)。资金计算范围包括现金替代标志为必须和退补的成分股所需资金、现金替代标志为允许的成分股缺口部分所需资金和预估现金差额。
            virtual double GetCreationValue() const = 0;
            //当前是否超过ETF最大现金替代比例(0-否，1-是) 
            virtual bool GetMaxCashRatioOver() const = 0;
            //ETF股票篮记录数
            virtual int64_t GetTotalRecordNum() const = 0;
            //成分股市场代码
            virtual uint16_t GetComponentMarketId() const = 0;
            //成分股代码
            virtual const char* GetComponentSecurityId() const = 0;
            //成分股简称
            virtual const char* GetComponentSecuritySymbol() const = 0;
            //证券类别
            virtual uint16_t GetSecurityType() const = 0;
            //现金替代标志
            virtual uint8_t GetSubstituteFlag() const = 0;
            //申购溢价比例N7(5)
            virtual double GetPremiumRatio() const = 0;
            //申购现金替代金额N15(4)
            virtual double GetPurchaseCashSubstitute() const = 0;
            //成分股总数量N15(2)，篮子数*成分股数量。（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetTotalComponentShareQty() const = 0;
            //可用于申购的股份数量N15(2)，仅对禁止现金替代和允许现金替代的成分股有效，其余现金替代标志固定为0。（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetEtfRedemptionQty() const = 0;
            //缺口数量N15(2)，成分股总数量与可用于申购的股份数量的差值，仅对禁止现金替代和允许现金替代的成分股有效，其余现金替代标志固定为0。如果成分股持仓足额，不存在缺口，则缺口数量固定为0。（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual double GetQtyGap() const = 0;
        };

        /**
         * @brief 现货外部实时资金查询消息
         */
        class QUANT_API ATPReqCashExtRealtimeFundQueryMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashExtRealtimeFundQueryMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashExtRealtimeFundQueryMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashExtRealtimeFundQueryMsg(const ATPReqCashExtRealtimeFundQueryMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashExtRealtimeFundQueryMsg& operator=(const ATPReqCashExtRealtimeFundQueryMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashExtRealtimeFundQueryMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashExtRealtimeFundQueryMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //客户密码（该字段是否必填请咨询券商）
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //货币种类
            virtual void SetCurrency(const char*) = 0;
            virtual const char* GetCurrency() const = 0;
        };

        /**
         * @brief 现货外部实时资金查询结果
         */
        class QUANT_API ATPRspCashExtRealtimeFundQueryResultMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashExtRealtimeFundQueryResultMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashExtRealtimeFundQueryResultMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashExtRealtimeFundQueryResultMsg(const ATPRspCashExtRealtimeFundQueryResultMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashExtRealtimeFundQueryResultMsg& operator=(const ATPRspCashExtRealtimeFundQueryResultMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPRspCashExtRealtimeFundQueryResultMsg* NewMessage();

            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPRspCashExtRealtimeFundQueryResultMsg* msg_ptr);

            /**
             * @brief 将消息结构编码为二进制数据流
             * @return Buffer 二进制数据流结构
             */
            virtual Buffer Encode() const = 0;

            /**
             * @brief 将二进制数据流解码为消息结构
             * @param[in] const Buffer& 二进制数据流结构
             * @return bool 解码是否成功
             */
            virtual bool Decode(const Buffer&) = 0;

        public:
            //营业部ID
            virtual const char* GetBranchId() const = 0;
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual const char* GetFundAccountId() const = 0;
            //货币种类
            virtual const char* GetCurrency() const = 0;
            //主柜资金余额N15(4)
            virtual double GetFundBalance() const = 0;
            //主柜可用资金N15(4)
            virtual double GetFundAvailable() const = 0;
            //主柜可取资金N15(4)
            virtual double GetFundFetch() const = 0;
        };

        /**
         * @brief 现货批量委托撤单消息
         */
        class QUANT_API ATPReqCashCancelBatchOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqCashCancelBatchOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqCashCancelBatchOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqCashCancelBatchOrderMsg(const ATPReqCashCancelBatchOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqCashCancelBatchOrderMsg& operator=(const ATPReqCashCancelBatchOrderMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqCashCancelBatchOrderMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqCashCancelBatchOrderMsg* msg_ptr);

        public:
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //批量委托母单编号，指被撤消订单的parent_cl_ord_no
            virtual void SetOrigParentClOrdNo(int64_t) = 0;
            virtual int64_t GetOrigParentClOrdNo() const = 0;
            //客户密码（该字段是否必填请咨询券商）
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //市场代码，开通北交所业务后必填（ATP V3.4版本开始支持）
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
        };

        /**
         * @brief 现货竞价批量委托消息
         */
        class QUANT_API ATPReqBatchCashAuctionOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPReqBatchCashAuctionOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPReqBatchCashAuctionOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPReqBatchCashAuctionOrderMsg(const ATPReqBatchCashAuctionOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPReqBatchCashAuctionOrderMsg& operator=(const ATPReqBatchCashAuctionOrderMsg&) = delete;
            /**
             * @brief 构建消息
             */
            static ATPReqBatchCashAuctionOrderMsg* NewMessage();
            /**
             * @brief 销毁消息
             * @param msg_ptr 待销毁的消息指针
             */
            static void DeleteMessage(ATPReqBatchCashAuctionOrderMsg* msg_ptr);

        public:
            //客户号ID
            virtual void SetCustId(const char*) = 0;
            virtual const char* GetCustId() const = 0;
            //资金账户ID
            virtual void SetFundAccountId(const char*) = 0;
            virtual const char* GetFundAccountId() const = 0;
            //营业部ID
            virtual void SetBranchId(const char*) = 0;
            virtual const char* GetBranchId() const = 0;
            //证券账户ID
            virtual void SetAccountId(const char*) = 0;
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual void SetMarketId(uint16_t) = 0;
            virtual uint16_t GetMarketId() const = 0;
            //客户密码（该字段是否必填请咨询券商）
            virtual void SetPassword(const char*) = 0;
            virtual const char* GetPassword() const = 0;
            //批量委托类型
            virtual void SetBatchType(uint8_t) = 0;
            virtual uint8_t GetBatchType() const = 0;
            //证券代码（当batch_type=3-多代码批量报单时，此字段非必填）
            virtual void SetSecurityId(const char*) = 0;
            virtual const char* GetSecurityId() const = 0;
            //买卖方向（当batch_type=3-多代码批量报单时，此字段非必填）
            virtual void SetSide(char) = 0;
            virtual char GetSide() const = 0;
            //申报数量N15(2)(股票为股、基金为份、上海债券默认为张(使用时请务必与券商确认)，其他为张)（当batch_type=3-多代码批量报单时，此字段非必填）
            virtual void SetOrderQty(double) = 0;
            virtual double GetOrderQty() const = 0;
            //委托价格N13(4)(上海市场股票、存托凭证、基金市价委托必须输入保护价)（当batch_type=3-多代码批量报单时，此字段非必填）
            virtual void SetPrice(double) = 0;
            virtual double GetPrice() const = 0;
            //订单类型（当batch_type=3-多代码批量报单时，此字段非必填） 
            virtual void SetOrderType(char) = 0;
            virtual char GetOrderType() const = 0;
            //单笔上限N15(2)（当batch_type=3-多代码批量报单时，此字段非必填）
            virtual void SetMaxOrderQty(double) = 0;
            virtual double GetMaxOrderQty() const = 0;
            //递减数量N15(2)（当batch_type=2-单代码递减拆单时，此字段必填）
            virtual void SetDecreaseQty(double) = 0;
            virtual double GetDecreaseQty() const = 0;
            //客户自定义委托批号(0为系统保留值，不允许使用)（ATP3.2.3开始支持现货集中竞价；ATP3.2.18开始支持ETF申赎）
            virtual void SetBatchClOrdNo(uint64_t) = 0;
            virtual uint64_t GetBatchClOrdNo() const = 0;

            //多证券委托组合（当batch_type=3-多代码批量报单时，必填）
            //重复组大小
            virtual uint32_t GetOrderArraySize() const = 0;
            //增加重复组
            virtual void AppendOrderArray() = 0;

            //证券代码
            virtual void SetOrderArraySecurityId(uint32_t, const char*) = 0;
            virtual const char* GetOrderArraySecurityId(uint32_t) const = 0;
            //买卖方向
            virtual void SetOrderArraySide(uint32_t, char) = 0;
            virtual char GetOrderArraySide(uint32_t) const = 0;
            //申报数量N15(2)（股票为股、基金为份、上海债券默认为张（使用时请务必与券商确认），其他为张）
            virtual void SetOrderArrayOrderQty(uint32_t, double) = 0;
            virtual double GetOrderArrayOrderQty(uint32_t) const = 0;
            //委托价格N13(4) （主板市价委托填0，科创板股票市价委托必须输入保护价）
            virtual void SetOrderArrayPrice(uint32_t, double) = 0;
            virtual double GetOrderArrayPrice(uint32_t) const = 0;
            //订单类型
            virtual void SetOrderArrayOrderType(uint32_t, char) = 0;
            virtual char GetOrderArrayOrderType(uint32_t) const = 0;
        };

        /**
         * @brief 现货批量订单委托响应
         */
        class QUANT_API ATPRspCashBatchAuctionOrderMsg
        {
        public:
            /**
             * @brief 构造函数
             */
            ATPRspCashBatchAuctionOrderMsg() = default;

            /**
             * @brief 析构函数
             */
            virtual ~ATPRspCashBatchAuctionOrderMsg() = default;

            /**
             * @brief 禁用拷贝构造
             */
            ATPRspCashBatchAuctionOrderMsg(const ATPRspCashBatchAuctionOrderMsg&) = delete;

            /**
             * @brief 禁用赋值构造
             */
            ATPRspCashBatchAuctionOrderMsg& operator=(const ATPRspCashBatchAuctionOrderMsg&) = delete;
        public:
            //客户号ID
            virtual const char* GetCustId() const = 0;
            //资金账号ID
            virtual const char* GetFundAccountId() const = 0;
            //证券账户ID
            virtual const char* GetAccountId() const = 0;
            //市场代码
            virtual uint16_t GetMarketId() const = 0;
            //证券代码
            virtual const char* GetSecurityId() const = 0;
            //批量委托类型
            virtual uint8_t GetBatchType() const = 0;
            //买卖方向
            virtual char GetSide() const = 0;
            //委托数量N15(2)
            virtual double GetOrderQty() const = 0;
            //委托价格N13(4)
            virtual double GetPrice() const = 0;
            //订单类型
            virtual char GetOrderType() const = 0;
            //母单订单编号
            virtual int64_t GetParentClOrdNo() const = 0;
            //客户自定义委托批号（ATP3.2.3开始支持现货集中竞价；ATP3.2.18开始支持ETF申赎）
            virtual uint64_t GetBatchClOrdNo() const = 0;
            //响应时间
            virtual int64_t GetTransactTime() const = 0;
        };

   } //namespace quant_api
} //namespace atp



#endif //  ATP_QUANT_MSG_H_