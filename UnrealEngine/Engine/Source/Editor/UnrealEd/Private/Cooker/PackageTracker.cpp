// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageTracker.h"

#include "CookOnTheFlyServerInterface.h"
#include "CookPackageData.h"
#include "CookPlatformManager.h"
#include "ProfilingDebugging/CookStats.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

namespace UE::Cook
{

#if ENABLE_COOK_STATS
	namespace Stats
	{
		// Stats tracked through FAutoRegisterCallback
		static uint32 NumInlineLoads = 0;
		static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
			{
				AddStat(TEXT("Package.Load"), FCookStatsManager::CreateKeyValueArray(TEXT("NumInlineLoads"), NumInlineLoads));
			});
	}
#endif

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


	FPackageTracker::FPackageTracker(FPackageDatas& InPackageDatas)
		:PackageDatas(InPackageDatas)
	{
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
	}

	FPackageTracker::~FPackageTracker()
	{
		GUObjectArray.RemoveUObjectDeleteListener(this);
		GUObjectArray.RemoveUObjectCreateListener(this);
	}

	TMap<UPackage*, FInstigator> FPackageTracker::GetNewPackages()
	{
		TMap<UPackage*, FInstigator> Result = MoveTemp(NewPackages);
		NewPackages.Reset();
		bHasBeenConsumed = true;
		return Result;
	}

	bool FPackageTracker::HasBeenConsumed() const
	{
		return bHasBeenConsumed;
	}

	void FPackageTracker::NotifyUObjectCreated(const class UObjectBase* Object, int32 Index)
	{
		if (Object->GetClass() == UPackage::StaticClass())
		{
			auto Package = const_cast<UPackage*>(static_cast<const UPackage*>(Object));

			if (Package->GetOuter() == nullptr)
			{
				LLM_SCOPE_BYTAG(Cooker);
				if (LoadingPackageData && Package->GetFName() != LoadingPackageData->GetPackageName())
				{
					COOK_STAT(++Stats::NumInlineLoads);
				}

				LoadedPackages.Add(Package);
				NewPackages.Add(Package,
						FInstigator(EInstigator::Unsolicited,
							LoadingPackageData ? LoadingPackageData->GetPackageName() : NAME_None)
					);
			}
		}
	}

	void FPackageTracker::NotifyUObjectDeleted(const class UObjectBase* Object, int32 Index)
	{
		if (Object->GetClass() == UPackage::StaticClass())
		{
			UPackage* Package = const_cast<UPackage*>(static_cast<const UPackage*>(Object));

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


}
