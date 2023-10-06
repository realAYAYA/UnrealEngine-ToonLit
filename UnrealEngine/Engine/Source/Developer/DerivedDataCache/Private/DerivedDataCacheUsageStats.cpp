// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataBackendInterface.h"

void GatherDerivedDataCacheResourceStats(TArray<FDerivedDataCacheResourceStat>& DDCResourceStats)
{
#if ENABLE_COOK_STATS
	// Temporary Set to allow accumulation by asset type
	TSet<FDerivedDataCacheResourceStat, FDerivedDataCacheResourceStatKeyFuncs> DDCResourceStatsSet;
	
	/** this functor will take a collected cooker stat and log it out using some custom formatting based on known stats that are collected.. */
	auto LogStatsFunc = [&DDCResourceStatsSet]
	(const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
	{
		if (StatName.EndsWith(TEXT(".Usage"), ESearchCase::IgnoreCase))
		{
			// Anything that ends in .Usage is assumed to be an instance of FCookStats.FDDCResourceUsageStats. We'll log that using custom formatting.
			FString AssetType = StatName;
			AssetType.RemoveFromEnd(TEXT(".Usage"), ESearchCase::IgnoreCase);
			// See if the asset has a subtype (found via the "Node" parameter")
			const FCookStatsManager::StringKeyValue* AssetSubType = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Node"); });
			if (AssetSubType && AssetSubType->Value.Len() > 0)
			{
				AssetType += FString::Printf(TEXT(" (%s)"), *AssetSubType->Value);
			}

			if (AssetType.Contains("DDC"))
				return;

			int64 AssetCount = 0;

			const FCookStatsManager::StringKeyValue* CountAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Count"); });

			if (CountAttr)
			{
				LexFromString(AssetCount, *CountAttr->Value);
			}

			if (AssetCount == 0)
				return;

			// Pull the Time and Size attributes and AddOrAccumulate them into the set of stats. Ugly string/container manipulation code courtesy of UE4/C++.
			const FCookStatsManager::StringKeyValue* AssetTimeSecAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("TimeSec"); });
			double AssetTimeSec = 0.0;

			if (AssetTimeSecAttr)
			{
				LexFromString(AssetTimeSec, *AssetTimeSecAttr->Value);
			}

			const FCookStatsManager::StringKeyValue* AssetSizeMBAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("MB"); });
			double AssetSizeMB = 0.0;

			if (AssetSizeMBAttr)
			{
				LexFromString(AssetSizeMB, *AssetSizeMBAttr->Value);
			}

			const FCookStatsManager::StringKeyValue* ThreadNameAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("ThreadName"); });
			bool bIsGameThreadTime = ThreadNameAttr != nullptr && ThreadNameAttr->Value == TEXT("GameThread");

			const FCookStatsManager::StringKeyValue* HitOrMissAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("HitOrMiss"); });
			bool bWasMiss = HitOrMissAttr != nullptr && HitOrMissAttr->Value == TEXT("Miss");

			const FCookStatsManager::StringKeyValue* Call = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Call"); });
			bool bWasGet = Call != nullptr && Call->Value == TEXT("Get");

			FDerivedDataCacheResourceStat Stat(AssetType);

			if (bWasMiss)
			{
				Stat = FDerivedDataCacheResourceStat(AssetType, bIsGameThreadTime, 0, 0, 0, AssetTimeSec, AssetSizeMB, AssetCount);
			}
			else
			{
				Stat = FDerivedDataCacheResourceStat(AssetType, bIsGameThreadTime, AssetTimeSec, AssetSizeMB, AssetCount, 0, 0, 0);
			}

			FDerivedDataCacheResourceStat* ExistingStat = DDCResourceStatsSet.Find(AssetType);

			if (ExistingStat)
			{
				*ExistingStat += Stat;
			}
			else
			{
				DDCResourceStatsSet.Add(Stat);
			}
		}
	};

	// Grab the DDC stats
	FCookStatsManager::LogCookStats(LogStatsFunc);

	// Place the Set members in the final Array
	for (const FDerivedDataCacheResourceStat& Stat : DDCResourceStatsSet)
	{
		DDCResourceStats.Add(Stat);
	}
#endif
}

void GatherDerivedDataCacheSummaryStats(FDerivedDataCacheSummaryStats& DDCSummaryStats)
{
#if ENABLE_COOK_STATS
	/** this functor will take a collected cooker stat and log it out using some custom formatting based on known stats that are collected.. */
	auto LogStatsFunc = [&DDCSummaryStats]
	(const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
	{
		if (StatName == TEXT("DDC.Summary"))
		{
			DDCSummaryStats.Stats.Append(StatAttributes);
		}
	};

	// Grab the DDC stats
	FCookStatsManager::LogCookStats(LogStatsFunc);
#endif
}

