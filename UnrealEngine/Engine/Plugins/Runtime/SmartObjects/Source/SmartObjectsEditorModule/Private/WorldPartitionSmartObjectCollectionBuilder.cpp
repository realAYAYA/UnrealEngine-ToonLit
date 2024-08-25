// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionSmartObjectCollectionBuilder.h"

#include "EditorBuildUtils.h"
#include "FileHelpers.h"
#include "HAL/PlatformFile.h"
#include "ISourceControlModule.h"
#include "Misc/App.h"
#include "SmartObjectComponent.h"
#include "PackageSourceControlHelper.h"
#include "SmartObjectSubsystem.h"
#include "UObject/SavePackage.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionSmartObjectCollectionBuilder)

#define LOCTEXT_NAMESPACE "SmartObjects"

UWorldPartitionSmartObjectCollectionBuilder::UWorldPartitionSmartObjectCollectionBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IterativeCellSize = 51200;
}

bool UWorldPartitionSmartObjectCollectionBuilder::PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{
	Super::PreRun(World, PackageHelper);

	USmartObjectSubsystem* SmartObjectSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(World);
	if (SmartObjectSubsystem == nullptr)
	{
		UE_LOG(LogSmartObject, Error, TEXT("SmartObjectSubsystem not found."));
		return false;
	}
	
	// We are about to rebuild all collections from scratch so we backup their
	// entry count and hash to dirty and save only the modified ones at the end of the run.
	NumSmartObjectsBefore.Reserve(SmartObjectSubsystem->GetRegisteredCollections().Num());
	OriginalContentsHash.Reserve(SmartObjectSubsystem->GetRegisteredCollections().Num());

	// Work on a copy of the registered components since it will be modified while unregistering/registering the collections.
	TArray<TWeakObjectPtr<ASmartObjectPersistentCollection>> RegisteredCollections(SmartObjectSubsystem->GetRegisteredCollections());
	for (const TWeakObjectPtr<ASmartObjectPersistentCollection>& WeakCollection : RegisteredCollections)
	{
		if (ASmartObjectPersistentCollection* Collection = WeakCollection.Get())
		{
			NumSmartObjectsBefore.Add(Collection->GetEntries().Num());
			OriginalContentsHash.Add(GetTypeHash(Collection->GetSmartObjectContainer()));

			// The subsystem container might contain entries related to the collection (if not empty) so we need to unregister it
			// to remove their entries and be able to registered it again after it has been reset.
			SmartObjectSubsystem->UnregisterCollection(*Collection);
			Collection->ResetCollection();
			SmartObjectSubsystem->RegisterCollection(*Collection);
		}
		else
		{
			NumSmartObjectsBefore.Add(0);
			OriginalContentsHash.Add(0);
		}
	}


	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (!WorldPartition)
	{
		UE_LOG(LogSmartObject, Error, TEXT("Failed to retrieve WorldPartition."));
		return false;
	}

	// parse the actors meta data to find the ones that contain smart objects
	TArray<USmartObjectComponent*> ExistingSOComponents;
	FWorldPartitionHelpers::ForEachActorDescInstance(WorldPartition, AActor::StaticClass(), [this, WorldPartition, &ExistingSOComponents](const FWorldPartitionActorDescInstance* ActorDescInstance)
	{
		if (ActorDescInstance->GetTags().Contains(UE::SmartObject::WithSmartObjectTag)
			&& ActorDescInstance->GetDataLayers().Num() > 0)
		{
			FWorldPartitionReference& ActorReference = SmartObjectReferences.Emplace_GetRef(WorldPartition, ActorDescInstance->GetGuid());
			if (AActor* Actor = ActorReference.GetActor())
			{
				if (USmartObjectComponent* SOComponent = Actor->GetComponentByClass<USmartObjectComponent>())
				{
					ExistingSOComponents.Add(SOComponent);
				}
			}
		}

		return true;
	});

	// manually register smart objects what we found via the meta data
	for (USmartObjectComponent* SOComponent : ExistingSOComponents)
	{
		SmartObjectSubsystem->RegisterSmartObject(*SOComponent);
	}

	return true;
}

bool UWorldPartitionSmartObjectCollectionBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	USmartObjectSubsystem* SmartObjectSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(World);
	if (SmartObjectSubsystem == nullptr)
	{
		// no need to report an error here, was already done in PreRun
		return false;
	}

	// this call will append all newly loaded smart objects to the appropriate collections
	SmartObjectSubsystem->IterativelyBuildCollections();

	return true;
}

bool UWorldPartitionSmartObjectCollectionBuilder::PostRun(UWorld* World, FPackageSourceControlHelper& PackageHelper, const bool bInRunSuccess)
{
	Super::PostRun(World, PackageHelper, bInRunSuccess);

	USmartObjectSubsystem* SmartObjectSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(World);
	if (SmartObjectSubsystem == nullptr)
	{
		return false;
	}

	bool bErrorsEncountered = !bInRunSuccess;

	const int32 CollectionsCount = SmartObjectSubsystem->GetMutableRegisteredCollections().Num();
	for (int32 CollectionIndex = 0; CollectionIndex < CollectionsCount; ++CollectionIndex)
	{
		ASmartObjectPersistentCollection* Collection = SmartObjectSubsystem->GetMutableRegisteredCollections()[CollectionIndex].Get();
		if (Collection == nullptr)
		{
			continue;
		}
	
		const int32 NumSmartObjectsAfter = Collection->GetEntries().Num();

		if (NumSmartObjectsBefore[CollectionIndex] == NumSmartObjectsAfter)
		{ 
			if (NumSmartObjectsAfter == 0 && bRemoveEmptyCollections == false)
			{
				// nothing to do here
				continue;
			}
			else
			{
				const uint32 NewHash = GetTypeHash(Collection->GetSmartObjectContainer());
				if (NewHash == OriginalContentsHash[CollectionIndex])
				{
					// Container hasn't changed. Skip as well.
					continue;
				}
			}
		}

		Collection->MarkPackageDirty();
		
		UPackage* Package = Collection->GetPackage();
		ensure(Package->IsDirty());

		const bool bDeletePackage = UPackage::IsEmptyPackage(Package) || (NumSmartObjectsAfter == 0 && bRemoveEmptyCollections);

		if (bDeletePackage)
		{
			UE_LOG(LogSmartObject, Log, TEXT("Deleting package %s."), *Package->GetName());
			if (!PackageHelper.Delete(Package))
			{
				UE_LOG(LogSmartObject, Error, TEXT("Error deleting package."));
				bErrorsEncountered = true;
			}
		}
		else
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
							bErrorsEncountered = true;
							continue;
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
				SaveArgs.SaveFlags = SAVE_None;
				if (!UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
				{
					UE_LOG(LogSmartObject, Error, TEXT("Error saving package %s."), *Package->GetName());
					bErrorsEncountered = true;
					continue;
				}
			}

			{
				// Add new packages to source control
				TRACE_CPUPROFILER_EVENT_SCOPE(AddingToSourceControl);
				UE_LOG(LogSmartObject, Log, TEXT("Adding package to source control."));

				if (!PackageHelper.AddToSourceControl(Package))
				{
					UE_LOG(LogSmartObject, Error, TEXT("Error adding package %s to source control."), *Package->GetName());
					bErrorsEncountered = true;
					continue;
				}
			}
		}
	}
	
	// unload the actors that contain smart objects
	SmartObjectReferences.Empty();

	return (bErrorsEncountered == false);
}

bool UWorldPartitionSmartObjectCollectionBuilder::CanBuildCollections(const UWorld* InWorld, FName BuildOption)
{
	return InWorld != nullptr && InWorld->IsPartitionedWorld();
}

EEditorBuildResult UWorldPartitionSmartObjectCollectionBuilder::BuildCollections(UWorld* InWorld, FName BuildOption)
{
	check(InWorld)
	
	const FString& PackageToReloadOnSuccess = GetNameSafe(InWorld->GetPackage());

	// Try to provide complete Path, if we can't try with project name
	const FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::GetProjectFilePath() : FApp::GetProjectName();

	const ISourceControlProvider& SCCProvider = ISourceControlModule::Get().GetProvider();

	const FString Arguments = FString::Printf(TEXT("\"%s\" -run=WorldPartitionBuilderCommandlet %s %s -SCCProvider=%s"),
			*ProjectPath,
			*PackageToReloadOnSuccess,
			TEXT(" -AllowCommandletRendering -Builder=WorldPartitionSmartObjectCollectionBuilder -log=WPSmartObjectCollectionBuilderLog.txt -logcmds=\"LogSmartObject VeryVerbose\""),
			*SCCProvider.GetName().ToString());
		
	const bool bSuccess = FEditorBuildUtils::RunWorldPartitionBuilder(PackageToReloadOnSuccess,
		LOCTEXT("WorldPartitionBuildSmartObjectCollectionProgress", "Building smart object collections..."),
		LOCTEXT("WorldPartitionBuildSmartObjectCollectionCancelled", "Building smart object collections cancelled!"),
		LOCTEXT("WorldPartitionBuildSmartObjectCollectionFailed", "Errors occured during the build process, please refer to the logs ('WPSmartObjectCollectionBuilderLog.txt')."),
		Arguments);
	
	return bSuccess ? EEditorBuildResult::Success : EEditorBuildResult::Skipped;
}

#undef LOCTEXT_NAMESPACE