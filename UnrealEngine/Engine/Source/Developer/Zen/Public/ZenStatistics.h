// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_WITH_ZEN

namespace UE::Zen {

	struct FZenCacheSizeStats
	{
		double Disk = 0;
		double Memory = 0;
	};

	struct FZenCacheStats
	{
		FZenCacheSizeStats Size;
		int64 Hits = 0;
		int64 Misses = 0;
		double HitRatio = 0.0;
		int64 UpstreamHits = 0;
		double UpstreamRatio = 0.0;	
	};

	struct FZenRequestStats
	{
		int64 Count = 0;
		double RateMean = 0.0;
		double TAverage = 0.0;
		double TMin = 0.0;
		double TMax = 0.0;
	};

	struct FZenEndPointStats
	{
		FString Name;
		FString Url;
		FString Health;
		double HitRatio = 0.0;
		double DownloadedMB = 0;
		double UploadedMB = 0;
		int64 ErrorCount = 0;
	};

	struct FZenUpstreamStats
	{
		bool Reading = false;
		bool Writing = false;
		int64 WorkerThreads = 0;
		int64 QueueCount = 0;
		double TotalUploadedMB = 0;
		double TotalDownloadedMB = 0;
		TArray<FZenEndPointStats> EndPointStats;
	};

	struct FZenCASSizeStats
	{
		int64 Tiny = 0;
		int64 Small = 0;
		int64 Large = 0;
		int64 Total = 0;
	};

	struct FZenCASStats
	{
		FZenCASSizeStats Size;
	};

	struct FZenStats
	{
		FZenRequestStats RequestStats;
		FZenCacheStats CacheStats;
		FZenUpstreamStats UpstreamStats;
		FZenRequestStats UpstreamRequestStats;
		FZenCASStats CASStats;
		bool IsValid = false;
	};

} // namespace UE::Zen

#endif // UE_WITH_ZEN
