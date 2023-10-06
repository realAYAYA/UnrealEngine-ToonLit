// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageTracker.h"

#include "CookOnTheFlyServerInterface.h"
#include "CookPackageData.h"
#include "CookPlatformManager.h"
#include "CookProfiling.h"
#include "Misc/ScopeRWLock.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

namespace UE::Cook
{

void FThreadSafeUnsolicitedPackagesList::AddCookedPackage(const FFilePlatformRequest& PlatformRequest)
{
	FScopeLock S(&SyncObject);
	CookedPackages.Add(PlatformRequest);
}

void FThreadSafeUnsolicitedPackagesList::GetPackagesForPlatformAndRemove(const ITargetPlatform* Platform, TArray<FName>& PackageNames)
{
	FScopeLock _(&SyncObject);

	for (int I = CookedPackages.Num() - 1; I >= 0; --I)
	{
		FFilePlatformRequest& Request = CookedPackages[I];

		if (Request.GetPlatforms().Contains(Platform))
		{
			// remove the platform
			Request.RemovePlatform(Platform);
			PackageNames.Emplace(Request.GetFilename());

			if (Request.GetPlatforms().Num() == 0)
			{
				CookedPackages.RemoveAt(I);
			}
		}
	}
}

void FThreadSafeUnsolicitedPackagesList::Empty()
{
	FScopeLock _(&SyncObject);
	CookedPackages.Empty();
}


FPackageTracker::FPackageTracker(UCookOnTheFlyServer& InCOTFS)
	:COTFS(InCOTFS)
{
}

FPackageTracker::~FPackageTracker()
{
	if (bTrackingInitialized)
	{
		GUObjectArray.RemoveUObjectDeleteListener(this);
		GUObjectArray.RemoveUObjectCreateListener(this);
	}
}

void FPackageTracker::InitializeTracking()
{
	check(!bTrackingInitialized);

	LLM_SCOPE_BYTAG(Cooker);

	FWriteScopeLock ScopeLock(Lock);
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;

		if (Package->GetOuter() == nullptr)
		{
			LoadedPackages.Add(Package);
		}
	}

	NewPackages.Reserve(LoadedPackages.Num());
	for (UPackage* Package : LoadedPackages)
	{
		NewPackages.Add(Package, FInstigator(EInstigator::StartupPackage));
	}

	GUObjectArray.AddUObjectDeleteListener(this);
	GUObjectArray.AddUObjectCreateListener(this);

	bTrackingInitialized = true;
}

TMap<UPackage*, FInstigator> FPackageTracker::GetNewPackages()
{
	if (!bTrackingInitialized)
	{
		InitializeTracking();
		check(bTrackingInitialized);
	}

	FWriteScopeLock ScopeLock(Lock);
	TMap<UPackage*, FInstigator> Result = MoveTemp(NewPackages);
	NewPackages.Reset();
	return Result;
}

void FPackageTracker::MarkLoadedPackagesAsNew()
{
	if (!bTrackingInitialized)
	{
		return;
	}

	LLM_SCOPE_BYTAG(Cooker);

	FWriteScopeLock ScopeLock(Lock);
	NewPackages.Reserve(LoadedPackages.Num());
	for (UPackage* Package : LoadedPackages)
	{
		NewPackages.FindOrAdd(Package, FInstigator(EInstigator::StartupPackage));
	}
}

void FPackageTracker::NotifyUObjectCreated(const class UObjectBase* Object, int32 Index)
{
	if (Object->GetClass() == UPackage::StaticClass())
	{
		auto Package = const_cast<UPackage*>(static_cast<const UPackage*>(Object));

		if (Package->GetOuter() == nullptr)
		{
			LLM_SCOPE_BYTAG(Cooker);
#if ENABLE_COOK_STATS
			++DetailedCookStats::NumDetectedLoads;
#endif
#if UE_WITH_PACKAGE_ACCESS_TRACKING
			PackageAccessTracking_Private::FTrackedData* AccumulatedScopeData = PackageAccessTracking_Private::FPackageAccessRefScope::GetCurrentThreadAccumulatedData();
			FName ReferencerName(AccumulatedScopeData ? AccumulatedScopeData->PackageName : NAME_None);
#else
			FName ReferencerName(NAME_None);
#endif
			EInstigator InstigatorType;
			switch (FCookLoadScope::GetCurrentValue())
			{
			case ECookLoadType::EditorOnly:
				InstigatorType = EInstigator::EditorOnlyLoad;
				break;
			case ECookLoadType::UsedInGame:
				InstigatorType = EInstigator::SaveTimeSoftDependency;
				break;
			default:
				InstigatorType = EInstigator::Unsolicited;
				break;
			}
			FInstigator Instigator(InstigatorType, ReferencerName);
			if (InstigatorType == EInstigator::Unsolicited && COTFS.bHiddenDependenciesDebug)
			{
				COTFS.OnDiscoveredPackageDebug(Package->GetFName(), Instigator);
			}

			FWriteScopeLock ScopeLock(Lock);
			if (ExpectedNeverLoadPackages.Contains(Package->GetFName()))
			{
				UE_LOG(LogCook, Verbose, TEXT("SoftGC PoorPerformance: Reloaded package %s."), *WriteToString<256>(Package->GetFName()));
			}
			LoadedPackages.Add(Package);
			NewPackages.Add(Package, MoveTemp(Instigator));

		}
	}
}

void FPackageTracker::NotifyUObjectDeleted(const class UObjectBase* Object, int32 Index)
{
	if (Object->GetClass() == UPackage::StaticClass())
	{
		UPackage* Package = const_cast<UPackage*>(static_cast<const UPackage*>(Object));

		FWriteScopeLock ScopeLock(Lock);
		LoadedPackages.Remove(Package);
		NewPackages.Remove(Package);
	}
}

void FPackageTracker::OnUObjectArrayShutdown()
{
	GUObjectArray.RemoveUObjectDeleteListener(this);
	GUObjectArray.RemoveUObjectCreateListener(this);
}

void FPackageTracker::RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap)
{
	RemapMapKeys(PlatformSpecificNeverCookPackages, Remap);
}

} // namespace UE::Cook
