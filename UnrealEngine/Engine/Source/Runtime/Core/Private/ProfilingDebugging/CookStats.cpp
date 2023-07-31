// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/CookStats.h"

#include "Trace/Trace.inl"

#if ENABLE_COOK_STATS

UE_TRACE_CHANNEL_DEFINE(CookChannel)

UE_TRACE_EVENT_BEGIN(CookTrace, Package)
	UE_TRACE_EVENT_FIELD(uint64, Id)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(CookTrace, PackageStat)
	UE_TRACE_EVENT_FIELD(uint64, Id)
	UE_TRACE_EVENT_FIELD(uint64, Duration)
	UE_TRACE_EVENT_FIELD(uint8, StatType)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(CookTrace, PackageAssetClass)
	UE_TRACE_EVENT_FIELD(uint64, Id)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, ClassName)
UE_TRACE_EVENT_END()

CORE_API FCookStatsManager::FGatherCookStatsDelegate FCookStatsManager::CookStatsCallbacks;

CORE_API void FCookStatsManager::LogCookStats(AddStatFuncRef AddStat)
{
	CookStatsCallbacks.Broadcast(AddStat);
}

void TracePackage(uint64 InId, const FStringView InName)
{
	UE_TRACE_LOG(CookTrace, Package, CookChannel)
		<< Package.Id(InId)
		<< Package.Name(InName.GetData(), uint16(InName.Len()))
		<< Package.Cycle(FPlatformTime::Cycles64());
}

void TracePackageStat(uint64 InId, uint64 Duration, EPackageEventStatType StatType)
{
	UE_TRACE_LOG(CookTrace, PackageStat, CookChannel)
		<< PackageStat.Id(InId)
		<< PackageStat.Duration(Duration)
		<< PackageStat.StatType((uint8)StatType);
}

void TracePackageAssetClass(uint64 InId, const FStringView InName)
{
	UE_TRACE_LOG(CookTrace, PackageAssetClass, CookChannel)
		<< PackageAssetClass.Id(InId)
		<< PackageAssetClass.ClassName(InName.GetData(), uint16(InName.Len()));
}

bool ShouldTracePackageInfo()
{
	return UE_TRACE_CHANNELEXPR_IS_ENABLED(CookChannel);
}

#endif
