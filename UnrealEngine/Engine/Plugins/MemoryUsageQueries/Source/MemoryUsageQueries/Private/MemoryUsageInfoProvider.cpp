// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemoryUsageInfoProvider.h"

uint64 FMemoryUsageInfoProviderLLM::GetAssetMemoryUsage(FName Asset, FOutputDevice* ErrorOutput /* = GLog */) const
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (FLowLevelMemTracker::IsEnabled())
	{
		return FLowLevelMemTracker::Get().GetTagAmountForTracker(ELLMTracker::Default, Asset, ELLMTagSet::Assets, UE::LLM::ESizeParams::Default);
	}
#endif
	ErrorOutput->Logf(TEXT("MemoryUsageInfoProvider Error: LLM is disabled. Please run with -LLM"));
	return 0U;
}

uint64 FMemoryUsageInfoProviderLLM::GetAssetsMemoryUsage(const TSet<FName>& Assets, FOutputDevice* ErrorOutput /* = GLog */) const
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (FLowLevelMemTracker::IsEnabled())
	{
		uint64 TotalSize = 0;
		for (const FName& Asset : Assets)
		{
			TotalSize += FLowLevelMemTracker::Get().GetTagAmountForTracker(ELLMTracker::Default, Asset, ELLMTagSet::Assets, UE::LLM::ESizeParams::Default);
		}
		return TotalSize;
	}
#endif
	ErrorOutput->Logf(TEXT("MemoryUsageInfoProvider Error: LLM is disabled. Please run with -LLM"));
	return 0U;
}

uint64 FMemoryUsageInfoProviderLLM::GetAssetsMemoryUsageWithSize(const TSet<FName>& Assets, TMap<FName, uint64>& OutSizes, FOutputDevice* ErrorOutput /* = GLog */) const
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (FLowLevelMemTracker::IsEnabled())
	{
		uint64 TotalSize = 0;
		for (const FName& Asset : Assets)
		{
			int64 AssetSize = FLowLevelMemTracker::Get().GetTagAmountForTracker(ELLMTracker::Default, Asset, ELLMTagSet::Assets, UE::LLM::ESizeParams::Default);
			OutSizes.Add(Asset, AssetSize);
			TotalSize += AssetSize;
		}
		return TotalSize;
	}
#endif
	ErrorOutput->Logf(TEXT("MemoryUsageInfoProvider Error: LLM is disabled. Please run with -LLM"));
	return 0U;
}

void FMemoryUsageInfoProviderLLM::GetAllAssetsWithSize(TMap<FName, uint64>& OutAssets, FOutputDevice* ErrorOutput /* = GLog */) const
{
	OutAssets.Reset();
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (FLowLevelMemTracker::IsEnabled())
	{
		FLowLevelMemTracker::Get().GetTrackedTagsNamesWithAmount(OutAssets, ELLMTracker::Default, ELLMTagSet::Assets);
		return;
	}
#endif
	ErrorOutput->Logf(TEXT("MemoryUsageInfoProvider Error: LLM is disabled. Please run with -LLM"));
}

#if ENABLE_LOW_LEVEL_MEM_TRACKER
void FMemoryUsageInfoProviderLLM::GetFilteredTagsWithSize(TMap<FName, uint64>& OutTags, ELLMTracker Tracker, ELLMTagSet TagSet, TArray<FLLMTagSetAllocationFilter>& Filters, FOutputDevice* ErrorOutput /* = GLog */) const
{
	OutTags.Reset();
	if (FLowLevelMemTracker::IsEnabled())
	{
		FLowLevelMemTracker::Get().GetTrackedTagsNamesWithAmountFiltered(OutTags, Tracker, TagSet, Filters);
		return;
	}
	ErrorOutput->Logf(TEXT("MemoryUsageInfoProvider Error: LLM is disabled. Please run with -LLM"));
}
#endif
