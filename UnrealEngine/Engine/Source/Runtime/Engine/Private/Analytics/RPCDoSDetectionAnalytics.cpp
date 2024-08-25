// Copyright Epic Games, Inc. All Rights Reserved.

// Includes

#include "Analytics/RPCDoSDetectionAnalytics.h"
#include "Net/RPCDoSDetectionConfig.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineLogs.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "HAL/IConsoleManager.h"


// Globals

FModifyRPCDoSAnalytics GModifyRPCDoSAnalytics;
FModifyRPCDoSAnalytics GModifyRPCDoSEscalationAnalytics;


/**
 * CVars
 */

TAutoConsoleVariable<int32> CVarRPCDoSAnalyticsMaxRPCs(
	TEXT("net.RPCDoSAnalyticsMaxRPCs"), 20,
	TEXT("The top 'x' number of RPC's to include in RPC DoS analytics, ranked by RPC rate per Second."));


namespace UE
{
	namespace Net
	{
		/**
		 * For Grafana template variable dropdown, restrict CPU values (including overshoot) to:
		 * 0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100, 105, 110 etc.
		 *
		 * @param InVal		The CPU Usage value to quantize
		 * @return			The quantized CPU Usage value
		 */
		uint8 GetQuantizedCPUUsage(uint8 InVal)
		{
			if (InVal >= 10 && InVal <= 90)
			{
				return (InVal / 10) * 10;
			}
			else
			{
				return (InVal / 5) * 5;
			}
		}
	}
}


/**
 * FRPCDoSAnalyticsVars
 */

FRPCDoSAnalyticsVars::FRPCDoSAnalyticsVars()
	: RPCTrackingAnalytics()
	, MaxRPCAnalytics(CVarRPCDoSAnalyticsMaxRPCs.GetValueOnAnyThread())
{
}

bool FRPCDoSAnalyticsVars::operator == (const FRPCDoSAnalyticsVars& A) const
{
	return PlayerIP == A.PlayerIP && PlayerUID == A.PlayerUID && MaxSeverityIndex == A.MaxSeverityIndex &&
			MaxSeverityCategory == A.MaxSeverityCategory && MaxAnalyticsSeverityIndex == A.MaxAnalyticsSeverityIndex &&
			MaxAnalyticsSeverityCategory == A.MaxAnalyticsSeverityCategory && RPCTrackingAnalytics == A.RPCTrackingAnalytics &&
			MaxPlayerSeverity == A.MaxPlayerSeverity;
}

void FRPCDoSAnalyticsVars::CommitAnalytics(FRPCDoSAnalyticsVars& AggregatedData)
{
	if (MaxSeverityIndex > AggregatedData.MaxSeverityIndex)
	{
		AggregatedData.MaxSeverityIndex = MaxSeverityIndex;
		AggregatedData.MaxSeverityCategory = MaxSeverityCategory;
	}

	if (MaxAnalyticsSeverityIndex > AggregatedData.MaxAnalyticsSeverityIndex)
	{
		AggregatedData.MaxAnalyticsSeverityIndex = MaxAnalyticsSeverityIndex;
		AggregatedData.MaxAnalyticsSeverityCategory = MaxAnalyticsSeverityCategory;
	}


	TArray<TSharedPtr<FRPCAnalytics>>& AggRPCTrackingAnalytics = AggregatedData.RPCTrackingAnalytics;
	AggRPCTrackingAnalytics.Append(RPCTrackingAnalytics);

	AggRPCTrackingAnalytics.Sort(
		[](const TSharedPtr<FRPCAnalytics>& A, const TSharedPtr<FRPCAnalytics>& B)
		{
			return A->MaxTimePerSec > B->MaxTimePerSec;
		});

	const int32 MaxSize = AggregatedData.MaxRPCAnalytics;

	if (AggRPCTrackingAnalytics.Num() > MaxSize)
	{
		AggRPCTrackingAnalytics.SetNum(MaxSize);
	}


	if (MaxAnalyticsSeverityIndex != 0)
	{
		TArray<FMaxRPCDoSEscalation>& AggMaxPlayerSeverity = AggregatedData.MaxPlayerSeverity;
		int32 SeverityInsertIdx = 0;

		for (; SeverityInsertIdx<AggMaxPlayerSeverity.Num(); SeverityInsertIdx++)
		{
			if (AggMaxPlayerSeverity[SeverityInsertIdx].MaxAnalyticsSeverityIndex < MaxAnalyticsSeverityIndex)
			{
				break;
			}
		}

		AggMaxPlayerSeverity.Insert({PlayerIP, PlayerUID, MaxSeverityIndex, MaxSeverityCategory, MaxAnalyticsSeverityIndex,
										MaxAnalyticsSeverityCategory},
										SeverityInsertIdx);
	}
}


/**
 * FRPCDoSAnalyticsData
 */

void FRPCDoSAnalyticsData::SendAnalytics()
{
	using namespace UE::Net;

	FRPCDoSAnalyticsVars NullVars;
	const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = Aggregator->GetAnalyticsProvider();

	if (!(*this == NullVars) && AnalyticsProvider.IsValid())
	{
		URPCDoSDetectionConfig* CurConfigObj = URPCDoSDetectionConfig::Get(Aggregator->GetNetDriverName());
		const bool bOverrideRPCThresholds = (CurConfigObj != nullptr && FMath::FRand() < CurConfigObj->RPCAnalyticsOverrideChance);

		auto WithinAnalyticsConfigThresholds =
			[CurConfigObj](const FRPCAnalytics& RPC) -> bool
			{
				bool bReturnVal = true;

				if (CurConfigObj != nullptr)
				{
					const FRPCAnalyticsThreshold* RPCThresholdConfig = CurConfigObj->RPCAnalyticsThresholds.FindByPredicate(
						[RPC](const FRPCAnalyticsThreshold& CurEntry)
						{
							return CurEntry.RPC == RPC.RPCName;
						});

					if (RPCThresholdConfig != nullptr && (RPCThresholdConfig->CountPerSec != -1 || RPCThresholdConfig->TimePerSec != 0.0))
					{
						const bool bHitCountThreshold = RPCThresholdConfig->CountPerSec != -1 && RPC.MaxCountPerSec > RPCThresholdConfig->CountPerSec;
						const bool bHitTimeThreshold = RPCThresholdConfig->TimePerSec != 0.0 && RPC.MaxTimePerSec > RPCThresholdConfig->TimePerSec;

						bReturnVal = bHitCountThreshold || bHitTimeThreshold;
					}
				}

				return bReturnVal;
			};

		// Remove RPC's from main analytics, which don't meet minimum thresholds
		TArray<TSharedPtr<FRPCAnalytics>> FilteredRPCs;

		for (int32 RPCIdx=RPCTrackingAnalytics.Num()-1; RPCIdx>=0; RPCIdx--)
		{
			const FRPCAnalytics& CurRPC = *RPCTrackingAnalytics[RPCIdx].Get();

			if (!CurRPC.WithinMinAnalyticsThreshold())
			{
				RPCTrackingAnalytics.RemoveAt(RPCIdx, 1, EAllowShrinking::No);
			}
			else if (!WithinAnalyticsConfigThresholds(CurRPC))
			{
				FilteredRPCs.Add(RPCTrackingAnalytics[RPCIdx]);
				RPCTrackingAnalytics.RemoveAt(RPCIdx, 1, EAllowShrinking::No);
			}
		}

		UE_LOG(LogNet, Log, TEXT("RPCDosDetection Analytics:"));

		UE_LOG(LogNet, Log, TEXT(" - MaxSeverityIndex: %i"), MaxSeverityIndex);
		UE_LOG(LogNet, Log, TEXT(" - MaxSeverityCategory: %s"), *MaxSeverityCategory);
		UE_LOG(LogNet, Log, TEXT(" - MaxAnalyticsSeverityIndex: %i"), MaxAnalyticsSeverityIndex);
		UE_LOG(LogNet, Log, TEXT(" - MaxAnalyticsSeverityCategory: %s"), *MaxAnalyticsSeverityCategory);
		UE_LOG(LogNet, Log, TEXT(" - RPCs: %i"), RPCTrackingAnalytics.Num());

		// NOTE: Game thread CPU must be in analytics, even if GTrackGameThreadCPUUsage == 0, as it's used for filtering.
		for (int32 RPCIdx=0; RPCIdx<RPCTrackingAnalytics.Num(); RPCIdx++)
		{
			const FRPCAnalytics& CurRPC = *RPCTrackingAnalytics[RPCIdx].Get();

			UE_LOG(LogNet, Log, TEXT("  - RPC[%i]:"), RPCIdx);
			UE_LOG(LogNet, Log, TEXT("   - Name: %s"), *CurRPC.RPCName.ToString());
			UE_LOG(LogNet, Log, TEXT("   - MaxCountPerSec: %i"), CurRPC.MaxCountPerSec);
			UE_LOG(LogNet, Log, TEXT("   - MaxTimePerSec: %f"), CurRPC.MaxTimePerSec);
			UE_LOG(LogNet, Log, TEXT("   - MaxTimeGameThreadCPU: %i"), CurRPC.MaxTimeGameThreadCPU);
			UE_LOG(LogNet, Log, TEXT("   - MaxSinglePacketRPCTime: %f"), CurRPC.MaxSinglePacketRPCTime);
			UE_LOG(LogNet, Log, TEXT("   - SinglePacketRPCCount: %i"), CurRPC.SinglePacketRPCCount);
			UE_LOG(LogNet, Log, TEXT("   - SinglePacketGameThreadCPU: %i"), CurRPC.SinglePacketGameThreadCPU);
			UE_LOG(LogNet, Log, TEXT("   - BlockedCount: %i"), CurRPC.BlockedCount);
			UE_LOG(LogNet, Log, TEXT("   - PlayerIP: %s"), *CurRPC.PlayerIP);
			UE_LOG(LogNet, Log, TEXT("   - PlayerUID: %s"), *CurRPC.PlayerUID);
		}

		UE_LOG(LogNet, Log, TEXT(" - FilteredRPCs: %i (bOverrideRPCThresholds: %i)"), FilteredRPCs.Num(), (int32)bOverrideRPCThresholds);

		for (int32 RPCIdx=0; RPCIdx<FilteredRPCs.Num(); RPCIdx++)
		{
			const FRPCAnalytics& CurRPC = *FilteredRPCs[RPCIdx].Get();

			UE_LOG(LogNet, Log, TEXT("  - FilteredRPC[%i]:"), RPCIdx);
			UE_LOG(LogNet, Log, TEXT("   - Name: (*) %s"), *CurRPC.RPCName.ToString());
			UE_LOG(LogNet, Log, TEXT("   - MaxCountPerSec: %i"), CurRPC.MaxCountPerSec);
			UE_LOG(LogNet, Log, TEXT("   - MaxTimePerSec: %f"), CurRPC.MaxTimePerSec);
			UE_LOG(LogNet, Log, TEXT("   - MaxTimeGameThreadCPU: %i"), CurRPC.MaxTimeGameThreadCPU);
			UE_LOG(LogNet, Log, TEXT("   - MaxSinglePacketRPCTime: %f"), CurRPC.MaxSinglePacketRPCTime);
			UE_LOG(LogNet, Log, TEXT("   - SinglePacketRPCCount: %i"), CurRPC.SinglePacketRPCCount);
			UE_LOG(LogNet, Log, TEXT("   - SinglePacketGameThreadCPU: %i"), CurRPC.SinglePacketGameThreadCPU);
			UE_LOG(LogNet, Log, TEXT("   - BlockedCount: %i"), CurRPC.BlockedCount);
			UE_LOG(LogNet, Log, TEXT("   - PlayerIP: %s"), *CurRPC.PlayerIP);
			UE_LOG(LogNet, Log, TEXT("   - PlayerUID: %s"), *CurRPC.PlayerUID);
		}

		UE_LOG(LogNet, Log, TEXT(" - MaxPlayerSeverity: %i"), MaxPlayerSeverity.Num());

		for (int32 SevIdx=0; SevIdx<MaxPlayerSeverity.Num(); SevIdx++)
		{
			const FMaxRPCDoSEscalation& CurSev = MaxPlayerSeverity[SevIdx];

			UE_LOG(LogNet, Log, TEXT("  - MaxPlayerSeverity[%i]:"), SevIdx);
			UE_LOG(LogNet, Log, TEXT("   - PlayerIP: %s"), *CurSev.PlayerIP);
			UE_LOG(LogNet, Log, TEXT("   - PlayerUID: %s"), *CurSev.PlayerUID);
			UE_LOG(LogNet, Log, TEXT("   - MaxSeverityIndex: %i"), CurSev.MaxSeverityIndex);
			UE_LOG(LogNet, Log, TEXT("   - MaxSeverityCategory: %s"), *CurSev.MaxSeverityCategory);
			UE_LOG(LogNet, Log, TEXT("   - MaxAnalyticsSeverityIndex: %i"), CurSev.MaxAnalyticsSeverityIndex);
			UE_LOG(LogNet, Log, TEXT("   - MaxAnalyticsSeverityCategory: %s"), *CurSev.MaxAnalyticsSeverityCategory);
		}


		static const FString EZEventName								= TEXT("Core.ServerRPCDoS");
		static const FString EZAttrib_MaxSeverityIndex					= TEXT("MaxSeverityIndex");
		static const FString EZAttrib_MaxSeverityCategory				= TEXT("MaxSeverityCategory");
		static const FString EZAttrib_MaxAnalyticsSeverityIndex			= TEXT("MaxAnalyticsSeverityIndex");
		static const FString EZAttrib_MaxAnalyticsSeverityCategory		= TEXT("MaxAnalyticsSeverityCategory");
		static const FString EZAttrib_RPCs								= TEXT("RPCs");
		static const FString EZAttrib_FilteredRPCs						= TEXT("FilteredRPCs");
		static const FString EZAttrib_RPCName							= TEXT("RPCName");
		static const FString EZAttrib_MaxCountPerSec					= TEXT("MaxCountPerSec");
		static const FString EZAttrib_MaxTimePerSec						= TEXT("MaxTimePerSec");
		static const FString EZAttrib_MaxTimeGameThreadCPU				= TEXT("MaxTimeGameThreadCPU");
		static const FString EZAttrib_MaxSinglePacketRPCTime			= TEXT("MaxSinglePacketRPCTime");
		static const FString EZAttrib_SinglePacketRPCCount				= TEXT("SinglePacketRPCCount");
		static const FString EZAttrib_SinglePacketGameThreadCPU			= TEXT("SinglePacketGameThreadCPU");
		static const FString EZAttrib_BlockedCount						= TEXT("BlockedCount");
		static const FString EZAttrib_PlayerIP							= TEXT("PlayerIP");
		static const FString EZAttrib_PlayerUID							= TEXT("PlayerUID");
		static const FString EZAttrib_MaxPlayerSeverity					= TEXT("MaxPlayerSeverity");


		// Json writer subclass to allow us to avoid using a SharedPtr to write basic Json (FN copy paste)
		typedef TCondensedJsonPrintPolicy<TCHAR> FPrintPolicy;
		class FAnalyticsJsonWriter : public TJsonStringWriter<FPrintPolicy>
		{
		public:
			explicit FAnalyticsJsonWriter(FString* Out) : TJsonStringWriter<FPrintPolicy>(Out, 0)
			{
			}
		};

		FString RPCsJsonStr;
		FAnalyticsJsonWriter RPCsJsonWriter(&RPCsJsonStr);

		RPCsJsonWriter.WriteArrayStart();

		for (int32 RPCIdx=0; RPCIdx<RPCTrackingAnalytics.Num(); RPCIdx++)
		{
			const FRPCAnalytics& CurRPC = *RPCTrackingAnalytics[RPCIdx].Get();

			RPCsJsonWriter.WriteObjectStart();

			RPCsJsonWriter.WriteValue(EZAttrib_RPCName, CurRPC.RPCName.ToString());
			RPCsJsonWriter.WriteValue(EZAttrib_MaxCountPerSec, CurRPC.MaxCountPerSec);
			RPCsJsonWriter.WriteValue(EZAttrib_MaxTimePerSec, CurRPC.MaxTimePerSec);
			RPCsJsonWriter.WriteValue(EZAttrib_MaxTimeGameThreadCPU, GetQuantizedCPUUsage(CurRPC.MaxTimeGameThreadCPU));
			RPCsJsonWriter.WriteValue(EZAttrib_MaxSinglePacketRPCTime, CurRPC.MaxSinglePacketRPCTime);
			RPCsJsonWriter.WriteValue(EZAttrib_SinglePacketRPCCount, CurRPC.SinglePacketRPCCount);
			RPCsJsonWriter.WriteValue(EZAttrib_SinglePacketGameThreadCPU, GetQuantizedCPUUsage(CurRPC.SinglePacketGameThreadCPU));
			RPCsJsonWriter.WriteValue(EZAttrib_BlockedCount, CurRPC.BlockedCount);
			RPCsJsonWriter.WriteValue(EZAttrib_PlayerIP, CurRPC.PlayerIP);
			RPCsJsonWriter.WriteValue(EZAttrib_PlayerUID, CurRPC.PlayerUID);

			RPCsJsonWriter.WriteObjectEnd();
		}

		RPCsJsonWriter.WriteArrayEnd();
		RPCsJsonWriter.Close();

		FString FilteredRPCsJsonStr;
		FAnalyticsJsonWriter FilteredRPCsJsonWriter(&FilteredRPCsJsonStr);

		FilteredRPCsJsonWriter.WriteArrayStart();

		if (bOverrideRPCThresholds)
		{
			for (int32 RPCIdx=0; RPCIdx<FilteredRPCs.Num(); RPCIdx++)
			{
				const FRPCAnalytics& CurRPC = *FilteredRPCs[RPCIdx].Get();

				FilteredRPCsJsonWriter.WriteObjectStart();

				TStringBuilder<512> RPCName;

				RPCName.Append(TEXT("(*) "));
				RPCName.Append(ToCStr(CurRPC.RPCName.ToString()));

				FilteredRPCsJsonWriter.WriteValue(EZAttrib_RPCName, RPCName.ToString());
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_MaxCountPerSec, CurRPC.MaxCountPerSec);
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_MaxTimePerSec, CurRPC.MaxTimePerSec);
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_MaxTimeGameThreadCPU, GetQuantizedCPUUsage(CurRPC.MaxTimeGameThreadCPU));
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_MaxSinglePacketRPCTime, CurRPC.MaxSinglePacketRPCTime);
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_SinglePacketRPCCount, CurRPC.SinglePacketRPCCount);
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_SinglePacketGameThreadCPU, GetQuantizedCPUUsage(CurRPC.SinglePacketGameThreadCPU));
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_BlockedCount, CurRPC.BlockedCount);
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_PlayerIP, CurRPC.PlayerIP);
				FilteredRPCsJsonWriter.WriteValue(EZAttrib_PlayerUID, CurRPC.PlayerUID);

				FilteredRPCsJsonWriter.WriteObjectEnd();
			}
		}

		FilteredRPCsJsonWriter.WriteArrayEnd();
		FilteredRPCsJsonWriter.Close();


		FString SevJsonStr;
		FAnalyticsJsonWriter SevJsonWriter(&SevJsonStr);

		SevJsonWriter.WriteArrayStart();

		for (int32 SevIdx=0; SevIdx<MaxPlayerSeverity.Num(); SevIdx++)
		{
			const FMaxRPCDoSEscalation& CurSev = MaxPlayerSeverity[SevIdx];

			SevJsonWriter.WriteObjectStart();

			SevJsonWriter.WriteValue(EZAttrib_PlayerIP, CurSev.PlayerIP);
			SevJsonWriter.WriteValue(EZAttrib_PlayerUID, CurSev.PlayerUID);
			SevJsonWriter.WriteValue(EZAttrib_MaxSeverityIndex, CurSev.MaxSeverityIndex);
			SevJsonWriter.WriteValue(EZAttrib_MaxSeverityCategory, CurSev.MaxSeverityCategory);
			SevJsonWriter.WriteValue(EZAttrib_MaxAnalyticsSeverityIndex, CurSev.MaxAnalyticsSeverityIndex);
			SevJsonWriter.WriteValue(EZAttrib_MaxAnalyticsSeverityCategory, CurSev.MaxAnalyticsSeverityCategory);

			SevJsonWriter.WriteObjectEnd();
		}

		SevJsonWriter.WriteArrayEnd();
		SevJsonWriter.Close();


		UWorld* World = WorldFunc ? WorldFunc() : nullptr;
		TArray<FAnalyticsEventAttribute> RPCDoSAttrs = MakeAnalyticsEventAttributeArray(
				EZAttrib_MaxSeverityIndex, MaxSeverityIndex,
				EZAttrib_MaxSeverityCategory, MaxSeverityCategory,
				EZAttrib_MaxAnalyticsSeverityIndex, MaxAnalyticsSeverityIndex,
				EZAttrib_MaxAnalyticsSeverityCategory, MaxAnalyticsSeverityCategory,
				EZAttrib_RPCs, FJsonFragment(MoveTemp(RPCsJsonStr)),
				EZAttrib_FilteredRPCs, FJsonFragment(MoveTemp(FilteredRPCsJsonStr)),
				EZAttrib_MaxPlayerSeverity, FJsonFragment(MoveTemp(SevJsonStr)));

		GModifyRPCDoSAnalytics.Broadcast(World, RPCDoSAttrs);

		AnalyticsProvider->RecordEvent(EZEventName, RPCDoSAttrs);
	}
}

void FRPCDoSAnalyticsData::FireEvent_ServerRPCDoSEscalation(int32 SeverityIndex, const FString& SeverityCategory, int32 WorstCountPerSec,
															double WorstTimePerSec, const FString& InPlayerIP, const FString& InPlayerUID,
															const TArray<FName>& InRPCGroup, double InRPCGroupTime/*=0.0*/)
{
	using namespace UE::Net;

	const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = Aggregator->GetAnalyticsProvider();

	if (AnalyticsProvider.IsValid())
	{
		static const FString EZEventName							= TEXT("Core.ServerRPCDoSEscalation");
		static const FString EZAttrib_AnalyticsSeverityIndex		= TEXT("AnalyticsSeverityIndex");
		static const FString EZAttrib_AnalyticsSeverityCategory		= TEXT("AnalyticsSeverityCategory");
		static const FString EZAttrib_WorstCountPerSec				= TEXT("WorstCountPerSec");
		static const FString EZAttrib_WorstTimePerSec				= TEXT("WorstTimePerSec");
		static const FString EZAttrib_PlayerIP						= TEXT("PlayerIP");
		static const FString EZAttrib_PlayerUID						= TEXT("PlayerUID");
		static const FString EZAttrib_GameThreadCPU					= TEXT("GameThreadCPU");
		static const FString EZAttrib_RPCGroup						= TEXT("RPCGroup");
		static const FString EZAttrib_RPCGroupTime					= TEXT("RPCGroupTime");

		UWorld* World = WorldFunc ? WorldFunc() : nullptr;
		TArray<FAnalyticsEventAttribute> RPCDoSEscalationAttrs = MakeAnalyticsEventAttributeArray(
				EZAttrib_AnalyticsSeverityIndex, SeverityIndex,
				EZAttrib_AnalyticsSeverityCategory, SeverityCategory,
				EZAttrib_WorstCountPerSec, WorstCountPerSec,
				EZAttrib_WorstTimePerSec, WorstTimePerSec,
				EZAttrib_PlayerIP, InPlayerIP,
				EZAttrib_PlayerUID, InPlayerUID,
				// NOTE: Game thread CPU must be in analytics, even if GTrackGameThreadCPUUsage == 0, as it's used for filtering.
				EZAttrib_GameThreadCPU, GetQuantizedCPUUsage(static_cast<uint8>(FPlatformTime::GetThreadCPUTime().CPUTimePctRelative)));

		if (InRPCGroup.Num() > 0)
		{
			TStringBuilder<2048> RPCGroupStr;

			for (const FName& CurRPC : InRPCGroup)
			{
				if (RPCGroupStr.Len() > 0)
				{
					RPCGroupStr.Append(TEXT(", "));
				}

				RPCGroupStr.Append(*CurRPC.ToString());
			}

			TArray<FAnalyticsEventAttribute> RPCGroupAttrs = MakeAnalyticsEventAttributeArray(
					EZAttrib_RPCGroup, RPCGroupStr.ToString(),
					EZAttrib_RPCGroupTime, InRPCGroupTime);

			RPCDoSEscalationAttrs.Append(RPCGroupAttrs);
		}

		GModifyRPCDoSEscalationAnalytics.Broadcast(World, RPCDoSEscalationAttrs);

		AnalyticsProvider->RecordEvent(EZEventName, RPCDoSEscalationAttrs);
	}
}

