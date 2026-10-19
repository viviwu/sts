#ifndef AMA_SERIALIZE_H_
#define AMA_SERIALIZE_H_

#include <string>
#include "ama_struct.h"
#include "ama_export.h"

namespace amd { namespace ama {

class AmaSerialize
{
public:
    static std::string Serialize(const MDSnapshot& snapshot);
    static std::string Serialize(const MDOptionSnapshot& snapshot);
    static std::string Serialize(const MDHKTSnapshot& snapshot);
    static std::string Serialize(const MDIndexSnapshot& snapshot);
    static std::string Serialize(const MDTickOrder& tick);
    static std::string Serialize(const MDTickExecution& tick);
    static std::string Serialize(const MDOrderQueue& orderqueue);
    static std::string Serialize(const MDOrderNums& ordernums);
    static std::string Serialize(const MDFutureSnapshot& snapshot);
    static std::string Serialize(const MDCSIIndexSnapshot& snapshot);
    static std::string Serialize(const MDAfterHourFixedPriceSnapshot& snapshot);
    static std::string Serialize(const MDAfterHourFixedPriceTickExecution& tick);
    static std::string Serialize(const MDIndicatorOfTradingVolumeSnapshot& snapshot);
    static std::string Serialize(const MDCnIndexSnapshot& snapshot);
    static std::string Serialize(const MDRefinancingTickOrder& tick);
    static std::string Serialize(const MDRefinancingTickExecution& tick);
    static std::string Serialize(const MDNegotiableTickOrder& tick);
    static std::string Serialize(const MDNegotiableTickExecution& tick);
    static std::string Serialize(const MDHKTRealtimeLimit& limit);
    static std::string Serialize(const MDHKTProductStatus& status);
    static std::string Serialize(const MDHKMarketStatus& status);
    static std::string Serialize(const MDHKTVCM& status);
    static std::string Serialize(const MDNEEQSnapshot& snapshot);
    static std::string Serialize(const MDNEEQSecurityInfo& info);
    static std::string Serialize(const MDNEEQNonPublicTransDeclaredInfo& info);
    static std::string Serialize(const MDNEEQHierarchicalInfo& info);
    static std::string Serialize(const MDNEEQNegotiableDeclaredInfo& info);
    static std::string Serialize(const MDNEEQMarketMakerDeclaredInfo& info);
    static std::string Serialize(const MDNEEQNonPublicTransferDealInfo& info);
    static std::string Serialize(const MDOrderBookSnapshot& snapshot);
    static std::string Serialize(const MDMinKLine& snapshot);
    static std::string Serialize(const MDIOPVSnapshot& snapshot);
    static std::string Serialize(const MDOrderBook& order_book);
    static std::string Serialize(const CodeTableRecord& record);
    static std::string Serialize(const MDBondSnapshot& snapshot);
    static std::string Serialize(const MDBondTickOrder& snapshot);
    static std::string Serialize(const MDBondQuotedTickOrder& snapshot);
    static std::string Serialize(const MDBondTickExecution& snapshot);
    static std::string Serialize(const MDBondQuotedTickExecution& snapshot);
    static std::string Serialize(const ETFCodeTableRecord& record);
    static std::string Serialize(const FutureAndOptionCodeTableRecord& record);
    static std::string Serialize(const HKExCodeTableRecord& record);
    static std::string Serialize(const MDFundExpertSnapshot& snapshot);
    static std::string Serialize(const MDHKExSnapshot& snapshot);
    static std::string Serialize(const MDHKExOrderSnapshot& snapshot);
    static std::string Serialize(const MDHKExMergeSnapshot& snapshot);
    static std::string Serialize(const MDHKExOrderBrokerSnapshot& snapshot);
    static std::string Serialize(const MDHKExTickExecution& tick);
    static std::string Serialize(const MDHKExIndexSnapshot& snapshot);
    static std::string Serialize(const MDHKExConnectTurnoverSnapshot& snapshot);
    static std::string Serialize(const IMCExchangeRate& rate);
    static std::string Serialize(const MDHKExCallAuctionSnapshot& snapshot);
    static std::string Serialize(const MDHKExStockOddLot& data);

    static std::string Serialize(const MDSpotTickExecution& snapshot);
    static std::string Serialize(const MDSpotSummarySnapshot& snapshot);
    static std::string Serialize(const MDSpotXBondSnapshot& snapshot);
    static std::string Serialize(const MDPledgeRepoTickExecution& snapshot);
    static std::string Serialize(const MDPledgeRepoSummarySnapshot& snapshot);
    static std::string Serialize(const MDPledgeRepoInquirySnapshot& snapshot);
    static std::string Serialize(const MDPledgeRepoDepositSnapshot& snapshot);
    static std::string Serialize(const MDPledgeRepoXRepoSnapshot& snapshot);
    static std::string Serialize(const MDCeditLoanTickExecution& snapshot);
    static std::string Serialize(const MDCeditLoanSnapshot& snapshot);
    static std::string Serialize(const MDCeditLoanDepositSnapshot& snapshot);
    static std::string Serialize(const MDSwapFixedFloatTickExecution& snapshot);
    static std::string Serialize(const MDSwapFixedFloatSnapshot& snapshot);
    static std::string Serialize(const MDSwapXSwapSnapshot& snapshot);
    static std::string Serialize(const MDSwapXSwapBestOfferSnapshot& snapshot);
    static std::string Serialize(const MDBondLoanTickExecution& snapshot);
    static std::string Serialize(const MDBondLoanUnderlyingSecurityRateSnapshot& snapshot);
    static std::string Serialize(const MDBondLoanTradingProductRateSnapshot& snapshot);
    static std::string Serialize(const MDSpotMarketMakersQuoteSnapshot& snapshot);
    static std::string Serialize(const MDSpotDirectiveQuoteSnapshot& snapshot);
    static std::string Serialize(const MDSpotXBondDepthQuoteSnapshot& snapshot);
    static std::string Serialize(const MDSpotXBondMatchQuoteSnapshot& snapshot);
    static std::string Serialize(const MDPledgeRepoXRepoDepthQuoteSnapshot& snapshot);
    static std::string Serialize(const MDPledgeRepoXRepoBestOfferQuoteSnapshot& snapshot);
    static std::string Serialize(const MDSwapXSwapDepthQuoteSnapshot& snapshot);
    static std::string Serialize(const MDSwapXSwapTickExecution& snapshot);
    static std::string Serialize(const MDBondInfoInterbank& data);
    static std::string Serialize(const MDABSInfo& data);
    static std::string Serialize(const MDABSHistoryInfo& data);
    static std::string Serialize(const MDABSCreditRatings& data);
    static std::string Serialize(const MDPreIssuedBondInfo& data);
    static std::string Serialize(const MDPreIPOBondInfo& data);
    static std::string Serialize(const MDXBondTradeBondInfo& data);
    static std::string Serialize(const MDPledgedConvertRateACInfo& data);
    static std::string Serialize(const MDXRepoHierQuoteGroupInfo& data);
    static std::string Serialize(const MDXRepoContractInfo& data);
    static std::string Serialize(const MDSwapFixedFloatInfo& data);
    static std::string Serialize(const MDSwapFixedFloatBasisContractInfo& data);
    static std::string Serialize(const MDBrokerBestOfferSnapshot& snapshot);
    static std::string Serialize(const MDBrokerTickExecution& tick);
    static std::string Serialize(const MDSpotXBondBestOfferQuoteSnapshot& snapshot);

    //美股
    static std::string Serialize(const MDUsaStockSnapshot& data);
    static std::string Serialize(const MDUsaOrderSnapshot& data);
    static std::string Serialize(const MDUsaEodSnapshot& data);

    //上海固收行情
    static std::string Serialize(const MDZQQDBJSnapshot& snapshot);
    static std::string Serialize(const MDZQCJHQSnapshot& snapshot);
    static std::string Serialize(const MDZQCJMXSnapshot& snapshot);
    static std::string Serialize(const MDUnmatchBondSnapshot& snapshot);
    static std::string Serialize(const MDUnmatchBondTickOrder& tick);
    static std::string Serialize(const MDUnmatchBondTickExecution& tick);
    
    //ADFMsg
    static std::string Serialize(const ADFMsg& adf);

    // 申万指数
    static std::string Serialize(const MDSWIndexSnapshot& snapshot);
};

}
}

#endif