// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/Model/CookProfilerProvider.h"
#include "CookProfilerProviderPrivate.h"
#include "AnalysisServicePrivate.h"

namespace TraceServices
{

thread_local FProviderLock::FThreadLocalState GCookProviderLockState;

const TCHAR* GUnknownPackage = TEXT("Unknown Package");
const TCHAR* GUnknownClass = TEXT("Unknown Class");

FPackageData::FPackageData(uint64 InId)
	: Id(InId)
	, Name(GUnknownPackage)
	, AssetClass(GUnknownClass)
{
}

FCookProfilerProvider::FCookProfilerProvider(IAnalysisSession& InSession)
	: Session(InSession)
{
}

void FCookProfilerProvider::EnumeratePackages(double StartTime, double EndTime, EnumeratePackagesCallback Callback) const
{
	ReadAccessCheck();

	for (const FPackageData& Package : Packages)
	{
		if (Callback(Package) == false)
		{
			break;
		}
	}
}

uint32 FCookProfilerProvider::GetNumPackages() const
{
	ReadAccessCheck();

	return (uint32) Packages.Num();
}

FPackageData* FCookProfilerProvider::EditPackage(uint64 Id)
{
	EditAccessCheck();

	uint32 Index = FindOrAddPackage(Id);

	FPackageData& Package = Packages[Index];
	return &Package;
}

uint32 FCookProfilerProvider::FindOrAddPackage(uint64 Id)
{
	uint32* Index = PackageIdToIndexMap.Find(Id);
	if (Index != nullptr)
	{
		return *Index;
	}
	else
	{
		PackageIdToIndexMap.Add(Id, static_cast<uint32>(Packages.Num()));
		Packages.Emplace(Id);
		return static_cast<uint32>(Packages.Num() - 1);
	}
}

} // namespace TraceServices
