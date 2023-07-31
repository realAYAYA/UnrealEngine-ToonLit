// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageEntry.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

FPackageEntry::FPackageEntry(const TraceServices::FPackageData& PackageData)
	: Id(PackageData.Id)
	, Name(PackageData.Name)
	, LoadTime(PackageData.LoadTime)
	, SaveTime(PackageData.SaveTime)
	, BeginCacheForCookedPlatformData(PackageData.BeginCacheForCookedPlatformData)
	, IsCachedCookedPlatformDataLoaded(PackageData.IsCachedCookedPlatformDataLoaded)
	, AssetClass(PackageData.AssetClass)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
