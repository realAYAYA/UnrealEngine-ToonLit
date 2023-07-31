// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "ProfilingDebugging/CookStats.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

/**
 * Usage stats for the derived data cache nodes. At the end of the app or commandlet, the DDC
 * can be asked to gather usage stats for each of the nodes in the DDC graph, which are accumulated
 * into a TMap of Name->Stats. The Stats portion is this class.
 * 
 * The class exposes various high-level routines to time important aspects of the DDC, mostly
 * focusing on performance of GetCachedData, PutCachedData, and CachedDataProbablyExists. This class
 * will track time taken, calls made, hits, misses, bytes processed, and do it for two buckets:
 * 1) the main thread and 2) all other threads.  The reason is because any time spent in the DDC on the
 * main thread is considered meaningful, as DDC access is generally expected to be async from helper threads.
 * 
 * The class goes through a bit of trouble to use thread-safe access calls for task-thread usage, and 
 * simple, fast accumulators for game-thread usage, since it's guaranteed to not be written to concurrently.
 * The class also limits itself to checking the thread once at construction.
 * 
 * Usage would be something like this in a concrete FDerivedDataBackendInterface implementation:
 *   class MyBackend : public FDerivedDataBackendInterface
 *   {
 *       FDerivedDataCacheUsageStats UsageStats;
 *   public:
 *       <override CachedDataProbablyExists>
 *       { 
 *           auto Timer = UsageStats.TimeExists();
 *           ...
 *       }
 *       <override GetCachedData>
 *       {    
 *           auto Timer = UsageStats.TimeGet();
 *           ...
 *           <if it's a cache hit> Timer.AddHit(DataSize);
 *           // Misses are automatically tracked
 *       }
 *       <override PutCachedData>
 *       {
 *           auto Timer = UsageStats.TimePut();
 *           ...
 *           <if the data will really be Put> Timer.AddHit(DataSize);
 *           // Misses are automatically tracked
 *       }
 *       <override GatherUsageStats>
 *       {
 *           // Add this node's UsageStats to the usage map. Your Key name should be UNIQUE to the entire graph (so use the file name, or pointer to this if you have to).
 *           UsageStatsMap.Add(FString::Printf(TEXT("%s: <Some unique name for this node instance>"), *GraphPath), UsageStats);
 *       }
*   }
 */
class FDerivedDataCacheUsageStats
{
#if ENABLE_COOK_STATS
public:

	/** Call this at the top of the CachedDataProbablyExists override. auto Timer = TimeProbablyExists(); */
	FCookStats::FScopedStatsCounter TimeProbablyExists()
	{
		return FCookStats::FScopedStatsCounter(ExistsStats);
	}

	/** Call this at the top of the GetCachedData override. auto Timer = TimeGet(); Use AddHit on the returned type to track a cache hit. */
	FCookStats::FScopedStatsCounter TimeGet()
	{
		return FCookStats::FScopedStatsCounter(GetStats);
	}

	/** Call this at the top of the PutCachedData override. auto Timer = TimePut(); Use AddHit on the returned type to track a cache hit. */
	FCookStats::FScopedStatsCounter TimePut()
	{
		return FCookStats::FScopedStatsCounter(PutStats);
	}

	/** Call this at the top of the PutCachedData override. auto Timer = TimePut(); Use AddHit on the returned type to track a cache hit. */
	FCookStats::FScopedStatsCounter TimePrefetch()
	{
		return FCookStats::FScopedStatsCounter(PrefetchStats);
	}

	void LogStats(FCookStatsManager::AddStatFuncRef AddStat, const FString& StatName, const FString& NodeName) const
	{
		GetStats.LogStats(AddStat, StatName, NodeName, TEXT("Get"));
		PutStats.LogStats(AddStat, StatName, NodeName, TEXT("Put"));
		ExistsStats.LogStats(AddStat, StatName, NodeName, TEXT("Exists"));
		PrefetchStats.LogStats(AddStat, StatName, NodeName, TEXT("Prefetch"));
	}

	void Combine(const FDerivedDataCacheUsageStats& Other)
	{
		GetStats.Combine(Other.GetStats);
		PutStats.Combine(Other.PutStats);
		ExistsStats.Combine(Other.ExistsStats);
		PrefetchStats.Combine(Other.PrefetchStats);
	}

	// expose these publicly for low level access. These should really never be accessed directly except when finished accumulating them.
	FCookStats::CallStats GetStats;
	FCookStats::CallStats PutStats;
	FCookStats::CallStats ExistsStats;
	FCookStats::CallStats PrefetchStats;
#endif
};

/** Performance stats for this backend */
struct FDerivedDataCacheSpeedStats
{
	double		ReadSpeedMBs = 0.0;
	double		WriteSpeedMBs = 0.0;
	double		LatencyMS = 0.0;
};

enum class EDerivedDataCacheStatus
{
	None = 0,
	Information,
	Warning,
	Error,
	Deactivation
};

/**
 *  Hierarchical usage stats for the DDC nodes.
 */
class FDerivedDataCacheStatsNode : public TSharedFromThis<FDerivedDataCacheStatsNode>
{
public:
	FDerivedDataCacheStatsNode() = default;

	FDerivedDataCacheStatsNode(const FString& InCacheType, const FString& InCacheName, bool bInIsLocal, EDerivedDataCacheStatus InCacheStatus = EDerivedDataCacheStatus::None, const TCHAR* InCacheStatusText = nullptr)
		: CacheType(InCacheType)
		, CacheName(InCacheName)
		, CacheStatusText(InCacheStatusText)
		, CacheStatus(InCacheStatus)
		, bIsLocal(bInIsLocal)
	{
	}

	const FString& GetCacheType() const { return CacheType; }

	const FString& GetCacheName() const { return CacheName; }

	const EDerivedDataCacheStatus GetCacheStatus() const { return CacheStatus; }

	const FString& GetCacheStatusText() const { return CacheStatusText; }

	bool IsLocal() const { return bIsLocal; }

	TMap<FString, FDerivedDataCacheUsageStats> ToLegacyUsageMap() const
	{
		TMap<FString, FDerivedDataCacheUsageStats> Stats;
		GatherLegacyUsageStats(Stats, TEXT(" 0"));
		return Stats;
	}

	void ForEachDescendant(TFunctionRef<void(TSharedRef<const FDerivedDataCacheStatsNode>)> Predicate) const
	{
		Predicate(SharedThis(this));

		for (const TSharedRef<FDerivedDataCacheStatsNode>& Child : Children)
		{
			Child->ForEachDescendant(Predicate);
		}
	}

public:
	void GatherLegacyUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStatsMap, FString&& GraphPath) const
	{
		if (UsageStats.Num() == 1)
		{
			for (const auto& KVP : UsageStats)
			{
				COOK_STAT(UsageStatsMap.Add(FString::Printf(TEXT("%s: %s"), *GraphPath, *GetCacheName()), KVP.Value));
			}
		}
		else
		{ //-V523
			for (const auto& KVP : UsageStats)
			{
				COOK_STAT(UsageStatsMap.Add(FString::Printf(TEXT("%s: %s.%s"), *GraphPath, *GetCacheName(), *KVP.Key), KVP.Value));
			}
		}

		int Ndx = 0;
		for (const TSharedRef<FDerivedDataCacheStatsNode>& Child : Children)
		{
			Child->GatherLegacyUsageStats(UsageStatsMap, GraphPath + FString::Printf(TEXT(".%2d"), Ndx++));
		}
	}

	TMap<FString, FDerivedDataCacheUsageStats> UsageStats;
	FDerivedDataCacheSpeedStats SpeedStats;

	TArray<TSharedRef<FDerivedDataCacheStatsNode>> Children;

protected:
	FString CacheType;
	FString CacheName;
	FString CacheStatusText;
	EDerivedDataCacheStatus CacheStatus;
	bool bIsLocal;
};

struct FDerivedDataCacheResourceStat
{
public:
	FDerivedDataCacheResourceStat(FString InAssetType = TEXT("None"), bool bIsGameThreadTime = 0.0, double InLoadTimeSec = 0.0, double InLoadSizeMB = 0.0, int64 InAssetsLoaded = 0, double InBuildTimeSec = 0.0, double InBuildSizeMB = 0.0, int64 InAssetsBuilt = 0) :
		AssetType(MoveTemp(InAssetType)),
		LoadTimeSec(InLoadTimeSec),
		LoadSizeMB(InLoadSizeMB),
		LoadCount(InAssetsLoaded),
		BuildTimeSec(InBuildTimeSec),
		BuildSizeMB(InBuildSizeMB),
		BuildCount(InAssetsBuilt),
		GameThreadTimeSec(bIsGameThreadTime ? InLoadTimeSec + InBuildTimeSec : 0.0)
	{}

	const FDerivedDataCacheResourceStat& operator+(const FDerivedDataCacheResourceStat& OtherStat)
	{
		GameThreadTimeSec += OtherStat.GameThreadTimeSec;

		LoadCount += OtherStat.LoadCount;
		LoadTimeSec += OtherStat.LoadTimeSec;
		LoadSizeMB += OtherStat.LoadSizeMB;

		BuildCount += OtherStat.BuildCount;
		BuildTimeSec += OtherStat.BuildTimeSec;
		BuildSizeMB += OtherStat.BuildSizeMB;

		return *this;
	}

	const FDerivedDataCacheResourceStat& operator-(const FDerivedDataCacheResourceStat& OtherStat)
	{
		GameThreadTimeSec -= OtherStat.GameThreadTimeSec;

		LoadCount -= OtherStat.LoadCount;
		LoadTimeSec -= OtherStat.LoadTimeSec;
		LoadSizeMB -= OtherStat.LoadSizeMB;

		BuildCount -= OtherStat.BuildCount;
		BuildTimeSec -= OtherStat.BuildTimeSec;
		BuildSizeMB -= OtherStat.BuildSizeMB;

		return *this;
	}

	const FDerivedDataCacheResourceStat& operator+=(const FDerivedDataCacheResourceStat& OtherStat)
	{
		*this = *this + OtherStat;
		return *this;
	}

	const FDerivedDataCacheResourceStat& operator-=(const FDerivedDataCacheResourceStat& OtherStat)
	{
		*this = *this - OtherStat;
		return *this;
	}

	FString AssetType;

	double LoadTimeSec;
	double LoadSizeMB;
	int64 LoadCount;

	double BuildTimeSec;
	double BuildSizeMB;
	int64 BuildCount;

	double GameThreadTimeSec;
};

struct FDerivedDataCacheResourceStatKeyFuncs : BaseKeyFuncs<FDerivedDataCacheResourceStat, FString, false>
{
	static const FString& GetSetKey(const FDerivedDataCacheResourceStat& Element) { return Element.AssetType; }
	static bool Matches(const FString& A, const FString& B) { return A == B; }
	static uint32 GetKeyHash(const FString& Key) { return GetTypeHash(Key); }
};

COOK_STAT(using FDerivedDataCacheSummaryStat = FCookStatsManager::StringKeyValue);

struct FDerivedDataCacheSummaryStats
{
	COOK_STAT(TArray<FDerivedDataCacheSummaryStat> Stats);
};
