// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionSmartObjectCollectionBuilder.h"

#include "EngineUtils.h"
#include "FileHelpers.h"
#include "SmartObjectCollection.h"
#include "WorldPartition/WorldPartition.h"
#include "SmartObjectSubsystem.h"
#include "UObject/SavePackage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionSmartObjectCollectionBuilder)

UWorldPartitionSmartObjectCollectionBuilder::UWorldPartitionSmartObjectCollectionBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), MainCollection(nullptr)
{
	IterativeCellSize = 51200;
}

bool UWorldPartitionSmartObjectCollectionBuilder::PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{
	Super::PreRun(World, PackageHelper);

	const USmartObjectSubsystem* SmartObjectSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(World);
	if (SmartObjectSubsystem == nullptr)
	{
		UE_LOG(LogSmartObject, Error, TEXT("SmartObjectSubsystem not found."));
		return false;
	}

	MainCollection = SmartObjectSubsystem->GetMainCollection();
	if (MainCollection == nullptr)
	{
		UE_LOG(LogSmartObject, Error, TEXT("Main SmartObject collection not found."));
		return false;
	}

	NumSmartObjectsBefore = MainCollection->GetEntries().Num();
	MainCollection->ResetCollection();
	MainCollection->SetBuildingForWorldPartition(true);

	NumSmartObjectsTotal = 0;

	return true;
}

bool UWorldPartitionSmartObjectCollectionBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	if (MainCollection == nullptr)
	{
		// no need to report an error here, was already done in PreRun
		return false;
	}

	const uint32 PreviousTotal = NumSmartObjectsTotal;
	NumSmartObjectsTotal = MainCollection->GetEntries().Num();

	ensureMsgf(NumSmartObjectsTotal >= PreviousTotal, TEXT("Collection is built incrementally so count should be stable or increase while loading new areas."));
	UE_CLOG(NumSmartObjectsTotal != PreviousTotal, LogSmartObject, Log, TEXT("Total = %d: added %d from area bounds [%s]"), NumSmartObjectsTotal, NumSmartObjectsTotal-PreviousTotal, *InCellInfo.Bounds.ToString());

	return true;
}

bool UWorldPartitionSmartObjectCollectionBuilder::PostRun(UWorld* World, FPackageSourceControlHelper& PackageHelper, const bool bInRunSuccess)
{
	Super::PostRun(World, PackageHelper, bInRunSuccess);

	if (MainCollection == nullptr)
	{
		// no need to report an error here, was already done in PreRun
		return false;
	}
	MainCollection->SetBuildingForWorldPartition(false);

	const bool bWasEmpty = NumSmartObjectsBefore == 0;
	const bool bIsEmpty = NumSmartObjectsTotal == 0;

	// If collection was empty and still is, nothing to save. Otherwise we mark as dirty.
	if (!(bIsEmpty && bWasEmpty))
	{
		MainCollection->MarkPackageDirty();
	}

	UPackage* Package = MainCollection->GetPackage();
	bool bDeletePackage = false;
	bool bSavePackage = false;
	if (Package->IsDirty())
	{
		if (UPackage::IsEmptyPackage(Package))
		{
			bDeletePackage = true;
		}
		else
		{
			bSavePackage = true;
		}
	}

	// Delete package
	if (bDeletePackage)
	{
		UE_LOG(LogSmartObject, Log, TEXT("Deleting package %s."), *Package->GetName());
		if (!PackageHelper.Delete(Package))
		{
			UE_LOG(LogSmartObject, Error, TEXT("Error deleting package."));
			return false;
		}
	}

	// Save packages
	if (bSavePackage)
	{
		{
			// Checkout package to save
			TRACE_CPUPROFILER_EVENT_SCOPE(CheckoutPackage);
			if (PackageHelper.UseSourceControl())
			{
				FEditorFileUtils::CheckoutPackages({Package}, /*OutPackagesCheckedOut*/nullptr, /*bErrorIfAlreadyCheckedOut*/false);
			}
			else
			{
				// Remove read-only
				const FString PackageFilename = SourceControlHelpers::PackageFilename(Package);
				if (IPlatformFile::GetPlatformPhysical().FileExists(*PackageFilename))
				{
					if (!IPlatformFile::GetPlatformPhysical().SetReadOnly(*PackageFilename, /*bNewReadOnlyValue*/false))
					{
						UE_LOG(LogSmartObject, Error, TEXT("Error setting %s writable"), *PackageFilename);
						return false;
					}
				}
			}
		}

		{
			// Save packages
			TRACE_CPUPROFILER_EVENT_SCOPE(SavingPackage);
			UE_LOG(LogSmartObject, Log, TEXT("   Saving package  %s."), *Package->GetName());
			const FString PackageFileName = SourceControlHelpers::PackageFilename(Package);
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			SaveArgs.SaveFlags = SAVE_Async;
			if (!UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
			{
				UE_LOG(LogSmartObject, Error, TEXT("Error saving package %s."), *Package->GetName());
				return false;
			}
		}

		{
			// Add new packages to source control
			TRACE_CPUPROFILER_EVENT_SCOPE(AddingToSourceControl);
			UE_LOG(LogSmartObject, Log, TEXT("Adding package to source control."));

			if (!PackageHelper.AddToSourceControl(Package))
			{
				UE_LOG(LogSmartObject, Error, TEXT("Error adding package %s to source control."), *Package->GetName());
				return false;
			}
		}

		UPackage::WaitForAsyncFileWrites();
	}

	return true;
}

