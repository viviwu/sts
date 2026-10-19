#ifndef _AMA_QUERY_SPI_H_
#define _AMA_QUERY_SPI_H_

#include "../ama_struct.h"
#include "../ama_export.h"

namespace amd { namespace ama {
/**
 * @brief AMA中的接收查询消息的回调基类，用户使用时需要继承该类。
 */
class AMA_EXPORT IQuerySpi
{
public:
    virtual ~IQuerySpi() {}

        /**
     * @brief                       接收逐笔委托数据查询回调
     *
     * @param item                  查询请求参数
     * @param tick                  逐笔委托数据
     * @param cnt                   逐笔委托数据条数
     * @param is_last               查询请求结束标识
     *
     * @attention                   使用后需要通过接口 IAMDApi::FreeMemory释放数据
     */
    virtual void OnMDTickOrder(const TickQueryItem& item, MDTickOrder* ticks, uint32_t cnt, bool is_last);

    /**
     * @brief                       接收逐笔成交数据查询回调
     *
     * @param item                  查询请求参数
     * @param tick                  逐笔成交数据
     * @param cnt                   逐笔成交数据条数
     * @param is_last               查询请求结束标识
     *
     * @attention                   使用后需要通过接口 IAMDApi::FreeMemory释放数据
     */
    virtual void OnMDTickExecution(const TickQueryItem& item, MDTickExecution* ticks, uint32_t cnt, bool is_last);


    /**
     * @brief                       接收逐笔成交数据查询异常回调
     *
     * @param item                  查询请求参数
     * @param error_code            查询错误码
     *
     */
    virtual void OnError(const TickQueryItem& item, int32_t error_code);


};

}  //end ama
}  //end amd

#endif