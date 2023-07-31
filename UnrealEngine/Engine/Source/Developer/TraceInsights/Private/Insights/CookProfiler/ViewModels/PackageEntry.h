// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraceServices/Model/CookProfilerProvider.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FPackageEntry
{
	friend class SPackageTableTreeView;

public:
	FPackageEntry(const TraceServices::FPackageData &PackageData);
	~FPackageEntry() {}

	uint64 GetId() const { return Id; }
	const TCHAR* GetName() const { return Name; }
	const double GetLoadTime() const { return LoadTime; }
	const double GetSaveTime() const { return SaveTime; }
	const double GetBeginCacheForCookedPlatformData() const { return BeginCacheForCookedPlatformData; }
	const double GetIsCachedCookedPlatformDataLoaded() const { return IsCachedCookedPlatformDataLoaded; }
	const TCHAR* GetAssetClass() const { return AssetClass; }

private:
	uint64 Id;
	const TCHAR* Name; 
	double LoadTime; 
	double SaveTime;
	double BeginCacheForCookedPlatformData;
	double IsCachedCookedPlatformDataLoaded;
	const TCHAR* AssetClass;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
