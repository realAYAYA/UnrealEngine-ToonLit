// Copyright Epic Games, Inc. All Rights Reserved.

// Includes
#include "OodleNetworkAnalytics.h"
#include "OodleNetworkHandlerComponent.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"


/**
 * FOodleNetworkAnalyticsVars
 */

FOodleNetworkAnalyticsVars::FOodleNetworkAnalyticsVars()
	: FLocalNetAnalyticsStruct()
	, InCompressedNum(0)
	, InNotCompressedNum(0)
	, InCompressedWithOverheadLengthTotal(0)
	, InCompressedLengthTotal(0)
	, InDecompressedLengthTotal(0)
	, InNotCompressedLengthTotal(0)
	, OutCompressedNum(0)
	, OutNotCompressedFailedNum(0)
	, OutNotCompressedFailedAckOnlyNum(0)
	, OutNotCompressedFailedKeepAliveNum(0)
	, OutNotCompressedBoundedNum(0)
	, OutNotCompressedFlaggedNum(0)
	, OutNotCompressedClientDisabledNum(0)
	, OutNotCompressedTooSmallNum(0)
	, OutCompressedWithOverheadLengthTotal(0)
	, OutCompressedLengthTotal(0)
	, OutBeforeCompressedLengthTotal(0)
	, OutNotCompressedFailedLengthTotal(0)
	, OutNotCompressedSkippedLengthTotal(0)
	, NumOodleNetworkHandlers(0)
	, NumOodleNetworkHandlersCompressionEnabled(0)
{
}

bool FOodleNetworkAnalyticsVars::operator == (const FOodleNetworkAnalyticsVars& A) const
{
	return A.InCompressedNum == InCompressedNum &&
		A.InNotCompressedNum == InNotCompressedNum &&
		A.InCompressedWithOverheadLengthTotal == InCompressedWithOverheadLengthTotal &&
		A.InCompressedLengthTotal == InCompressedLengthTotal &&
		A.InDecompressedLengthTotal == InDecompressedLengthTotal &&
		A.InNotCompressedLengthTotal == InNotCompressedLengthTotal &&
		A.OutCompressedNum == OutCompressedNum &&
		A.OutNotCompressedFailedNum == OutNotCompressedFailedNum &&
		A.OutNotCompressedFailedAckOnlyNum == OutNotCompressedFailedAckOnlyNum &&
		A.OutNotCompressedFailedKeepAliveNum == OutNotCompressedFailedKeepAliveNum &&
		A.OutNotCompressedBoundedNum == OutNotCompressedBoundedNum &&
		A.OutNotCompressedFlaggedNum == OutNotCompressedFlaggedNum &&
		A.OutNotCompressedClientDisabledNum == OutNotCompressedClientDisabledNum &&
		A.OutNotCompressedTooSmallNum == OutNotCompressedTooSmallNum &&
		A.OutCompressedWithOverheadLengthTotal == OutCompressedWithOverheadLengthTotal &&
		A.OutCompressedLengthTotal == OutCompressedLengthTotal &&
		A.OutBeforeCompressedLengthTotal == OutBeforeCompressedLengthTotal &&
		A.OutNotCompressedFailedLengthTotal == OutNotCompressedFailedLengthTotal &&
		A.OutNotCompressedSkippedLengthTotal == OutNotCompressedSkippedLengthTotal &&
		A.NumOodleNetworkHandlers == NumOodleNetworkHandlers &&
		A.NumOodleNetworkHandlersCompressionEnabled == NumOodleNetworkHandlersCompressionEnabled;
}

void FOodleNetworkAnalyticsVars::CommitAnalytics(FOodleNetworkAnalyticsVars& AggregatedData)
{
	AggregatedData.InCompressedNum += InCompressedNum;
	AggregatedData.InNotCompressedNum += InNotCompressedNum;
	AggregatedData.InCompressedWithOverheadLengthTotal += InCompressedWithOverheadLengthTotal;
	AggregatedData.InCompressedLengthTotal += InCompressedLengthTotal;
	AggregatedData.InDecompressedLengthTotal += InDecompressedLengthTotal;
	AggregatedData.InNotCompressedLengthTotal += InNotCompressedLengthTotal;
	AggregatedData.OutCompressedNum += OutCompressedNum;
	AggregatedData.OutNotCompressedFailedNum += OutNotCompressedFailedNum;
	AggregatedData.OutNotCompressedFailedAckOnlyNum += OutNotCompressedFailedAckOnlyNum;
	AggregatedData.OutNotCompressedFailedKeepAliveNum += OutNotCompressedFailedKeepAliveNum;
	AggregatedData.OutNotCompressedBoundedNum += OutNotCompressedBoundedNum;
	AggregatedData.OutNotCompressedFlaggedNum += OutNotCompressedFlaggedNum;
	AggregatedData.OutNotCompressedClientDisabledNum += OutNotCompressedClientDisabledNum;
	AggregatedData.OutNotCompressedTooSmallNum = OutNotCompressedTooSmallNum;
	AggregatedData.OutCompressedWithOverheadLengthTotal += OutCompressedWithOverheadLengthTotal;
	AggregatedData.OutCompressedLengthTotal += OutCompressedLengthTotal;
	AggregatedData.OutBeforeCompressedLengthTotal += OutBeforeCompressedLengthTotal;
	AggregatedData.OutNotCompressedFailedLengthTotal += OutNotCompressedFailedLengthTotal;
	AggregatedData.OutNotCompressedSkippedLengthTotal += OutNotCompressedSkippedLengthTotal;
	AggregatedData.NumOodleNetworkHandlers += NumOodleNetworkHandlers;
	AggregatedData.NumOodleNetworkHandlersCompressionEnabled += NumOodleNetworkHandlersCompressionEnabled;
}


/**
 * FOodleNetAnalyticsData
 */

void FOodleNetAnalyticsData::SendAnalytics()
{
	FOodleNetworkAnalyticsVars NullVars;
	const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = Aggregator->GetAnalyticsProvider();

	// Only send analytics if there is something to send
	if (!(*this == NullVars) && AnalyticsProvider.IsValid())
	{
		/** The number of outgoing packets that were not compressed, in total */
		uint64 OutNotCompressedNumTotal = OutNotCompressedFailedNum + OutNotCompressedBoundedNum + OutNotCompressedFlaggedNum + OutNotCompressedClientDisabledNum + OutNotCompressedTooSmallNum;

		/** The number of outgoing packets that had compression skipped, in total */
		uint64 OutNotCompressedSkippedNum = OutNotCompressedFlaggedNum + OutNotCompressedClientDisabledNum + OutNotCompressedTooSmallNum;


		uint64 InPreLengthTotal = InCompressedLengthTotal + InNotCompressedLengthTotal;
		uint64 InPreWithOverheadLengthTotal = InCompressedWithOverheadLengthTotal + InNotCompressedLengthTotal;
		uint64 InPostLengthTotal = InDecompressedLengthTotal + InNotCompressedLengthTotal;

		uint64 OutPreLengthTotal = OutBeforeCompressedLengthTotal + OutNotCompressedFailedLengthTotal + OutNotCompressedSkippedLengthTotal;
		uint64 OutPostLengthTotal = OutCompressedLengthTotal + OutNotCompressedFailedLengthTotal + OutNotCompressedSkippedLengthTotal;
		uint64 OutPostWithOverheadLengthTotal = OutCompressedWithOverheadLengthTotal + OutNotCompressedFailedLengthTotal + OutNotCompressedSkippedLengthTotal;

		uint64 OutPreAttemptedLengthTotal = OutBeforeCompressedLengthTotal + OutNotCompressedFailedLengthTotal;
		uint64 OutPostAttemptedWithOverheadLengthTotal = OutCompressedWithOverheadLengthTotal + OutNotCompressedFailedLengthTotal;


		// @todo #JohnB: Deprecate these four algorithm-based analytics, eventually - since skipped packets render them inaccurate
		/**
		 * The below values measure Oodle algorithm compression, minus overhead reducing final savings.
		 * Also factors in skipped/failed compression (which has increased a lot since analytics was added) - reducing the usefulness for determining algorithm compression.
		 */
			/** The percentage of compression savings, of all incoming packets. */
			int8 InSavingsPercentTotal = (1.0 - ((double)InPreLengthTotal / (double)InPostLengthTotal)) * 100.0;

			/** The percentage of compression savings, of all outgoing packets. */
			int8 OutSavingsPercentTotal = (1.0 - ((double)OutPostLengthTotal / (double)OutPreLengthTotal)) * 100.0;

			/** The number of bytes saved due to compression, of all incoming packets. */
			int64 InSavingsBytesTotal = InPostLengthTotal - InPreLengthTotal;

			/** The number of bytes saved due to compression, of all outgoing packets. */
			int64 OutSavingsBytesTotal = OutPreLengthTotal - OutPostLengthTotal;

		/**
		 * The below values measure compressed length + decompression data overhead, which reduces final savings.
		 * This is the most accurate measure of compression savings (in terms of overall bandwidth).
		 */
			/** The percentage of compression savings, of all incoming packets. */
			int8 InSavingsWithOverheadPercentTotal = (1.0 - ((double)InPreWithOverheadLengthTotal / (double)InPostLengthTotal)) * 100.0;

			/** The percentage of compression savings, of all outgoing packets. */
			int8 OutSavingsWithOverheadPercentTotal = (1.0 - ((double)OutPostWithOverheadLengthTotal / (double)OutPreLengthTotal)) * 100.0;

			/** The number of bytes saved due to compression, of all incoming packets. */
			int64 InSavingsWithOverheadBytesTotal = InPostLengthTotal - InPreWithOverheadLengthTotal;

			/** The number of bytes saved due to compression, of all outgoing packets. */
			int64 OutSavingsWithOverheadBytesTotal = OutPreLengthTotal - OutPostWithOverheadLengthTotal;

		/**
		 * The below values measure compressed length + decompression data overhead, only for packets where compression was attempted (i.e. ignores skipped packets).
		 * This is the best measure for determining algorithm compression performance, especially when measured against the Oodle encode/decode CPU cost.
		 */
			/** The percentage of compression savings, of all incoming packets that were compressed (attempted but failed compress, can't be counted here) */
			int8 InAttemptedSavingsWithOverheadPercentTotal = (1.0 - ((double)InCompressedWithOverheadLengthTotal / (double)InDecompressedLengthTotal)) * 100.0;

			/** The percentage of compression savings, of all outgoing packets that attempted compression. */
			int8 OutAttemptedSavingsWithOverheadPercentTotal = (1.0 - ((double)OutPostAttemptedWithOverheadLengthTotal / (double)OutPreAttemptedLengthTotal)) * 100.0;


		uint32 NumOodleNetworkHandlersCompressionDisabled = NumOodleNetworkHandlers - NumOodleNetworkHandlersCompressionEnabled;


		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT("OodleNetwork Analytics:"));
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InCompressedNum: %llu"), InCompressedNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InNotCompressedNum: %llu"), InNotCompressedNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InCompressedWithOverheadLengthTotal: %llu"), InCompressedWithOverheadLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InCompressedLengthTotal: %llu"), InCompressedLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InDecompressedLengthTotal: %llu"), InDecompressedLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InNotCompressedLengthTotal: %llu"), InNotCompressedLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutCompressedNum: %llu"), OutCompressedNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedFailedNum: %llu"), OutNotCompressedFailedNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedFailedAckOnlyNum: %llu"), OutNotCompressedFailedAckOnlyNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedFailedKeepAliveNum: %llu"), OutNotCompressedFailedKeepAliveNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedBoundedNum: %llu"), OutNotCompressedBoundedNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedFlaggedNum: %llu"), OutNotCompressedFlaggedNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedSkippedNum: %llu"), OutNotCompressedSkippedNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedClientDisabledNum: %llu"), OutNotCompressedClientDisabledNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedTooSmallNum: %llu"), OutNotCompressedTooSmallNum);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutCompressedWithOverheadLengthTotal: %llu"), OutCompressedWithOverheadLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutCompressedLengthTotal: %llu"), OutCompressedLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutBeforeCompressedLengthTotal: %llu"), OutBeforeCompressedLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedFailedLengthTotal: %llu"), OutNotCompressedFailedLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedSkippedLengthTotal: %llu"), OutNotCompressedSkippedLengthTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutNotCompressedNumTotal: %llu"), OutNotCompressedNumTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InSavingsPercentTotal: %i"), InSavingsPercentTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutSavingsPercentTotal: %i"), OutSavingsPercentTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InSavingsBytesTotal: %lli"), InSavingsBytesTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutSavingsBytesTotal: %lli"), OutSavingsBytesTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InSavingsWithOverheadPercentTotal: %i"), InSavingsWithOverheadPercentTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutSavingsWithOverheadPercentTotal: %i"), OutSavingsWithOverheadPercentTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InSavingsWithOverheadBytesTotal: %lli"), InSavingsWithOverheadBytesTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutSavingsWithOverheadBytesTotal: %lli"), OutSavingsWithOverheadBytesTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - InAttemptedSavingsWithOverheadPercentTotal: %i"), InAttemptedSavingsWithOverheadPercentTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - OutAttemptedSavingsWithOverheadPercentTotal: %i"), OutAttemptedSavingsWithOverheadPercentTotal);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - NumOodleNetworkHandlers: %i"), NumOodleNetworkHandlers);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - NumOodleNetworkHandlersCompressionEnabled: %i"), NumOodleNetworkHandlersCompressionEnabled);
		UE_LOG(OodleNetworkHandlerComponentLog, Log, TEXT(" - NumOodleNetworkHandlersCompressionDisabled: %i"), NumOodleNetworkHandlersCompressionDisabled);


		static const FString EZAttrib_InCompressedNum = TEXT("InCompressedNum");
		static const FString EZAttrib_InNotCompressedNum = TEXT("InNotCompressedNum");
		static const FString EZAttrib_InCompressedWithOverheadLengthTotal = TEXT("InCompressedWithOverheadLengthTotal");
		static const FString EZAttrib_InCompressedLengthTotal = TEXT("InCompressedLengthTotal");
		static const FString EZAttrib_InDecompressedLengthTotal = TEXT("InDecompressedLengthTotal");
		static const FString EZAttrib_InNotCompressedLengthTotal = TEXT("InNotCompressedLengthTotal");
		static const FString EZAttrib_OutCompressedNum = TEXT("OutCompressedNum");
		static const FString EZAttrib_OutNotCompressedFailedNum = TEXT("OutNotCompressedFailedNum");
		static const FString EZAttrib_OutNotCompressedFailedAckOnlyNum = TEXT("OutNotCompressedFailedAckOnlyNum");
		static const FString EZAttrib_OutNotCompressedFailedKeepAliveNum = TEXT("OutNotCompressedFailedKeepAliveNum");
		static const FString EZAttrib_OutNotCompressedBoundedNum = TEXT("OutNotCompressedBoundedNum");
		static const FString EZAttrib_OutNotCompressedFlaggedNum = TEXT("OutNotCompressedFlaggedNum");
		static const FString EZAttrib_OutNotCompressedSkippedNum = TEXT("OutNotCompressedSkippedNum");
		static const FString EZAttrib_OutNotCompressedClientDisabledNum = TEXT("OutNotCompressedClientDisabledNum");
		static const FString EZAttrib_OutNotCompressedTooSmallNum = TEXT("OutNotCompressedTooSmallNum");
		static const FString EZAttrib_OutCompressedWithOverheadLengthTotal = TEXT("OutCompressedWithOverheadLengthTotal");
		static const FString EZAttrib_OutCompressedLengthTotal = TEXT("OutCompressedLengthTotal");
		static const FString EZAttrib_OutBeforeCompressedLengthTotal = TEXT("OutBeforeCompressedLengthTotal");
		static const FString EZAttrib_OutNotCompressedFailedLengthTotal = TEXT("OutNotCompressedFailedLengthTotal");
		static const FString EZAttrib_OutNotCompressedSkippedLengthTotal = TEXT("OutNotCompressedSkippedLengthTotal");
		static const FString EZAttrib_OutNotCompressedNumTotal = TEXT("OutNotCompressedNumTotal");
		static const FString EZAttrib_InSavingsPercentTotal = TEXT("InSavingsPercentTotal");
		static const FString EZAttrib_OutSavingsPercentTotal = TEXT("OutSavingsPercentTotal");
		static const FString EZAttrib_InSavingsBytesTotal = TEXT("InSavingsBytesTotal");
		static const FString EZAttrib_OutSavingsBytesTotal = TEXT("OutSavingsBytesTotal");
		static const FString EZAttrib_InSavingsWithOverheadPercentTotal = TEXT("InSavingsWithOverheadPercentTotal");
		static const FString EZAttrib_OutSavingsWithOverheadPercentTotal = TEXT("OutSavingsWithOverheadPercentTotal");
		static const FString EZAttrib_InSavingsWithOverheadBytesTotal = TEXT("InSavingsWithOverheadBytesTotal");
		static const FString EZAttrib_OutSavingsWithOverheadBytesTotal = TEXT("OutSavingsWithOverheadBytesTotal");
		static const FString EZAttrib_InAttemptedSavingsWithOverheadPercentTotal = TEXT("InAttemptedSavingsWithOverheadPercentTotal");
		static const FString EZAttrib_OutAttemptedSavingsWithOverheadPercentTotal = TEXT("OutAttemptedSavingsWithOverheadPercentTotal");
		static const FString EZAttrib_NumOodleNetworkHandlers = TEXT("NumOodleNetworkHandlers");
		static const FString EZAttrib_NumOodleNetworkHandlersCompressionEnabled = TEXT("NumOodleNetworkHandlersCompressionEnabled");
		static const FString EZAttrib_NumOodleNetworkHandlersCompressionDisabled = TEXT("NumOodleNetworkHandlersCompressionDisabled");

		const TArray<FAnalyticsEventAttribute> EventAttributes = MakeAnalyticsEventAttributeArray(
			EZAttrib_InCompressedNum, InCompressedNum,
			EZAttrib_InNotCompressedNum, InNotCompressedNum,
			EZAttrib_InCompressedWithOverheadLengthTotal, InCompressedWithOverheadLengthTotal,
			EZAttrib_InCompressedLengthTotal, InCompressedLengthTotal,
			EZAttrib_InDecompressedLengthTotal, InDecompressedLengthTotal,
			EZAttrib_InNotCompressedLengthTotal, InNotCompressedLengthTotal,
			EZAttrib_OutCompressedNum, OutCompressedNum,
			EZAttrib_OutNotCompressedFailedNum, OutNotCompressedFailedNum,
			EZAttrib_OutNotCompressedFailedAckOnlyNum, OutNotCompressedFailedAckOnlyNum,
			EZAttrib_OutNotCompressedFailedKeepAliveNum, OutNotCompressedFailedKeepAliveNum,
			EZAttrib_OutNotCompressedBoundedNum, OutNotCompressedBoundedNum,
			EZAttrib_OutNotCompressedFlaggedNum, OutNotCompressedFlaggedNum,
			EZAttrib_OutNotCompressedSkippedNum, OutNotCompressedSkippedNum,
			EZAttrib_OutNotCompressedClientDisabledNum, OutNotCompressedClientDisabledNum,
			EZAttrib_OutNotCompressedTooSmallNum, OutNotCompressedTooSmallNum,
			EZAttrib_OutCompressedWithOverheadLengthTotal, OutCompressedWithOverheadLengthTotal,
			EZAttrib_OutCompressedLengthTotal, OutCompressedLengthTotal,
			EZAttrib_OutBeforeCompressedLengthTotal, OutBeforeCompressedLengthTotal,
			EZAttrib_OutNotCompressedFailedLengthTotal, OutNotCompressedFailedLengthTotal,
			EZAttrib_OutNotCompressedSkippedLengthTotal, OutNotCompressedSkippedLengthTotal,
			EZAttrib_OutNotCompressedNumTotal, OutNotCompressedNumTotal,
			EZAttrib_InSavingsPercentTotal, InSavingsPercentTotal,
			EZAttrib_OutSavingsPercentTotal, OutSavingsPercentTotal,
			EZAttrib_InSavingsBytesTotal, InSavingsBytesTotal,
			EZAttrib_OutSavingsBytesTotal, OutSavingsBytesTotal,
			EZAttrib_InSavingsWithOverheadPercentTotal, InSavingsWithOverheadPercentTotal,
			EZAttrib_OutSavingsWithOverheadPercentTotal, OutSavingsWithOverheadPercentTotal,
			EZAttrib_InSavingsWithOverheadBytesTotal, InSavingsWithOverheadBytesTotal,
			EZAttrib_OutSavingsWithOverheadBytesTotal, OutSavingsWithOverheadBytesTotal,
			EZAttrib_InAttemptedSavingsWithOverheadPercentTotal, InAttemptedSavingsWithOverheadPercentTotal,
			EZAttrib_OutAttemptedSavingsWithOverheadPercentTotal, OutAttemptedSavingsWithOverheadPercentTotal,
			EZAttrib_NumOodleNetworkHandlers, NumOodleNetworkHandlers,
			EZAttrib_NumOodleNetworkHandlersCompressionEnabled, NumOodleNetworkHandlersCompressionEnabled,
			EZAttrib_NumOodleNetworkHandlersCompressionDisabled, NumOodleNetworkHandlersCompressionDisabled
		);

		AnalyticsProvider->RecordEvent(GetAnalyticsEventName(), EventAttributes);
	}
}

const TCHAR* FOodleNetAnalyticsData::GetAnalyticsEventName() const
{
	return TEXT("Oodle.Stats");
}

const TCHAR* FClientOodleNetAnalyticsData::GetAnalyticsEventName() const
{
	return TEXT("Oodle.ClientStats");
}
