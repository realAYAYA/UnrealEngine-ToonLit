// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpBlend_FrustumCache.h"

void FDisplayClusterWarpBlend_FrustumCache::SetFrustumCacheDepth(const int32 InFrustumCacheDepth)
{
	check(IsInGameThread());

	FrustumCacheDepth = InFrustumCacheDepth;
}

void FDisplayClusterWarpBlend_FrustumCache::SetFrustumCachePrecision(float InFrustumCachePrecision)
{
	check(IsInGameThread());

	FrustumCachePrecision = InFrustumCachePrecision;
}

bool FDisplayClusterWarpBlend_FrustumCache::GetCachedFrustum(const FDisplayClusterWarpEye& InEye, FDisplayClusterWarpContext& OutContext)
{
	check(IsInGameThread());

	if (FrustumCacheDepth == 0)
	{
		// Cache disabled, clear old values
		ResetFrustumCache();
	}
	else
	{
		// Try to use old frustum values from cache (reduce CPU cost)
		if (FrustumCache.Num() > 0)
		{
			for (int32 FrustumCacheIndex = 0; FrustumCacheIndex < FrustumCache.Num(); FrustumCacheIndex++)
			{
				if (FrustumCache[FrustumCacheIndex].Eye.IsEqual(InEye, FrustumCachePrecision))
				{
					// Use cached value
					OutContext = FrustumCache[FrustumCacheIndex].Context;

					// Move used value on a cache top
					AddFrustum(InEye, OutContext);
					return true;
				}
			}
		}
	}

	return false;
}

void FDisplayClusterWarpBlend_FrustumCache::AddFrustum(const FDisplayClusterWarpEye& InEye, const FDisplayClusterWarpContext& InContext)
{
	check(IsInGameThread());

	// Store current used frustum value to cache
	if (FrustumCacheDepth > 0)
	{
		FrustumCache.Add(FCacheItem(InEye, InContext));
	}

	// Remove too old cached values
	const int32 TotalTooOldValuesCount = FrustumCache.Num() - FrustumCacheDepth;
	if (TotalTooOldValuesCount > 0)
	{
		FrustumCache.RemoveAt(0, TotalTooOldValuesCount);
	}
}

void FDisplayClusterWarpBlend_FrustumCache::ResetFrustumCache()
{
	check(IsInGameThread());

	FrustumCache.Empty();
}

