// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/CookProfiler/ViewModels/PackageEntry.h"
#include "Insights/Table/ViewModels/Table.h"

namespace Insights
{

class FTableColumn;

////////////////////////////////////////////////////////////////////////////////////////////////////

// Column identifiers
struct FPackageTableColumns
{
	static const FName IdColumnId;
	static const FName NameColumnId;
	static const FName LoadTimeColumnId;
	static const FName SaveTimeColumnId;
	static const FName BeginCacheForCookedPlatformDataTimeColumnId;
	static const FName GetIsCachedCookedPlatformDataLoadedColumnId;
	static const FName PackageAssetClassColumnId;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FPackageTable : public FTable
{
public:
	FPackageTable();
	virtual ~FPackageTable();

	virtual void Reset();

	TArray<FPackageEntry>& GetPackageEntries() { return PackageEntries; }
	const TArray<FPackageEntry>& GetPackageEntries() const { return PackageEntries; }

	bool IsValidRowIndex(int32 InIndex) const { return InIndex >= 0 && InIndex < PackageEntries.Num(); }
	const FPackageEntry* GetPackage(int32 InIndex) const { return IsValidRowIndex(InIndex) ? &PackageEntries[InIndex] : nullptr; }
	const FPackageEntry& GetPackageChecked(int32 InIndex) const { check(IsValidRowIndex(InIndex)); return PackageEntries[InIndex]; }

private:
	void AddDefaultColumns();

private:
	TArray<FPackageEntry> PackageEntries;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
