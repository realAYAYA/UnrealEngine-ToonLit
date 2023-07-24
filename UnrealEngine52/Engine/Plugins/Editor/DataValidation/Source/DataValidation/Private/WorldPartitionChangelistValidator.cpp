// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionChangelistValidator.h"
#include "AssetRegistry/ARFilter.h"
#include "DataValidationChangelist.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ISourceControlModule.h"
#include "Misc/PackageName.h"
#include "SourceControlHelpers.h"

#include "Engine/Level.h"
#include "Misc/PathViews.h"
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"
#include "WorldPartition/WorldPartitionActorDescView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionChangelistValidator)

#define LOCTEXT_NAMESPACE "WorldPartitionChangelistValidation"

bool UWorldPartitionChangelistValidator::CanValidateAsset_Implementation(UObject* InAsset) const
{
	return (InAsset != nullptr) && (UDataValidationChangelist::StaticClass() == InAsset->GetClass());
}

EDataValidationResult UWorldPartitionChangelistValidator::ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionChangelistValidator::ValidateLoadedAsset_Implementation);

	UDataValidationChangelist* ChangeList = CastChecked<UDataValidationChangelist>(InAsset);
	
	Errors = &ValidationErrors;
	
	EDataValidationResult Result = ValidateActorsAndDataLayersFromChangeList(ChangeList);

	if (Result == EDataValidationResult::Invalid)
	{
		AssetFails(InAsset, LOCTEXT("WorldPartitionValidationFail", "This changelist contains modifications that aren't valid at the world partition level. Please see revision control log and correct the errors."), ValidationErrors);
	}
	else
	{
		AssetPasses(InAsset);
	}
		
	return Result;
}

// Extract all Actors/Map from Changelist (in OFPA this should be one Actor per Package, and we'll discard all Actors from non WorldPartition maps)
// and add them to a Map of World->Files[] so that we can do one validation per world. Once Worlds are identified, we either the UActorDescContainer 
// from memory (if loaded) or request it to be loaded, we then build a Set of objects that interest us from the Actors in the CL 
EDataValidationResult UWorldPartitionChangelistValidator::ValidateActorsAndDataLayersFromChangeList(UDataValidationChangelist* Changelist)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlChangelistStatePtr ChangelistState = SourceControlProvider.GetState(Changelist->Changelist->AsShared(), EStateCacheUsage::Use);

	// Figure out which world(s) these actors are in and split the files per world, and return the actor's native class
	TMap<FTopLevelAssetPath, TSet<FAssetData>> MapToActorsFiles;
	auto TryAssociateActorToMap = [&MapToActorsFiles](const FAssetData& AssetData) -> UClass*
	{
		// Check that the asset is an actor
		if (FWorldPartitionActorDescUtils::IsValidActorDescriptorFromAssetData(AssetData))
		{
			// WorldPartition actors are all in OFPA mode so they're external
			// Extract the MapName from the ObjectPath (<PathToPackage>.<MapName>:<Level>.<ActorName>)
			FSoftObjectPath ActorPath = AssetData.GetSoftObjectPath();
			FTopLevelAssetPath MapAssetName = ActorPath.GetAssetPath();

			TSet<FAssetData>* ActorFiles = MapToActorsFiles.Find(MapAssetName);

			if (!ActorFiles)
			{
				if (ULevel::GetIsLevelPartitionedFromPackage(ActorPath.GetLongPackageFName()))
				{
					ActorFiles = &MapToActorsFiles.Add(MapAssetName);
				}
			}

			if (ActorFiles)	// A null Files indicates a World not using World Partition and OFPA 
			{
				ActorFiles->Add(AssetData);
				return FWorldPartitionActorDescUtils::GetActorNativeClassFromAssetData(AssetData);;
			}
		}

		return nullptr;
	};
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	for (const FSourceControlStateRef& File : ChangelistState->GetFilesStates())
	{		
		// Skip deleted files since we're not validating references in this validator 
		if (File->IsDeleted())
		{
			continue;
		}

		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(File->GetFilename(), PackageName))
		{						
			TArray<FAssetData> PackageAssetsData;
			USourceControlHelpers::GetAssetDataFromPackage(PackageName, PackageAssetsData);
			
			for (FAssetData& AssetData : PackageAssetsData)
			{
				if (UClass* ActorNativeClass = TryAssociateActorToMap(AssetData))
				{
					SubmittingWorldDataLayers = ActorNativeClass->IsChildOf<AWorldDataLayers>();
				}
				else if (UClass* AssetClass = AssetData.GetClass())
				{
					if (AssetClass->IsChildOf<UDataLayerAsset>())
					{
						TArray<FName> ReferencerNames;
						AssetRegistry.GetReferencers(AssetData.PackageName, ReferencerNames, UE::AssetRegistry::EDependencyCategory::All);
			
						FARFilter Filter;
						Filter.bIncludeOnlyOnDiskAssets = true;
						Filter.PackageNames = MoveTemp(ReferencerNames);
						Filter.ClassPaths.Add(AWorldDataLayers::StaticClass()->GetClassPathName());

						TArray<FAssetData> DataLayerReferencers;
						AssetRegistry.GetAssets(Filter, DataLayerReferencers);

						for (const FAssetData& DataLayerReferencer : DataLayerReferencers)
						{
							TryAssociateActorToMap(DataLayerReferencer);
						}
			
						RelevantDataLayerAssets.Add(AssetData.PackageName.ToString());
					}
					else if (AssetClass->IsChildOf<UWorld>())
					{
						if (ULevel::GetIsLevelPartitionedFromPackage(*PackageName))
						{
							MapToActorsFiles.FindOrAdd(AssetData.GetSoftObjectPath().GetAssetPath());
						}
					}
				}
			}
		}
	}

	auto RegisterContainerToValidate = [](UWorld* InWorld, FName InContainerPackageName, FActorDescContainerCollection& OutRegisteredContainers)
	{
		if (OutRegisteredContainers.Contains(InContainerPackageName))
		{
			return;
		}

		UActorDescContainer* ActorDescContainer = nullptr;
		if (InWorld != nullptr)
		{
			// World is Loaded reuse the ActorDescContainer of the Content Bundle
			ActorDescContainer = InWorld->GetWorldPartition()->FindContainer(InContainerPackageName);
		}

		// Even if world is valid, its world partition is not necessarily initialized
		if (!ActorDescContainer)
		{
			// Find in memory failed, load the ActorDescContainer
			ActorDescContainer = NewObject<UActorDescContainer>();
			ActorDescContainer->Initialize({ nullptr, InContainerPackageName });
		}

		OutRegisteredContainers.AddContainer(ActorDescContainer);
	};

	// For Each world 
	for (TTuple<FTopLevelAssetPath, TSet<FAssetData>>& It : MapToActorsFiles)
	{
		const FTopLevelAssetPath& MapPath = It.Get<0>();
		const TSet<FAssetData>& ActorsData = It.Get<1>();

		// Find/Load the ActorDescContainer
		UWorld* World = FindObject<UWorld>(nullptr, *MapPath.ToString(), true);
		
		FActorDescContainerCollection ContainersToValidate;
		for (const FAssetData& ActorData : ActorsData)
		{
			FString ActorPackagePath = ActorData.PackagePath.ToString();
			if (ContentBundlePaths::IsAContentBundlePackagePath(ActorPackagePath))
			{
				FStringView ContentBundleMountPoint = FPathViews::GetMountPointNameFromPath(ActorPackagePath);
				FGuid ContentBundleGuid = ContentBundlePaths::GetContentBundleGuidFromExternalActorPackagePath(ActorPackagePath);
				
				FString ContentBundleContainerPackagePath;
				verify(ContentBundlePaths::BuildActorDescContainerPackgePath(FString(ContentBundleMountPoint), ContentBundleGuid, MapPath.GetPackageName().ToString(), ContentBundleContainerPackagePath));

				RegisterContainerToValidate(World, FName(*ContentBundleContainerPackagePath), ContainersToValidate);
			}
			else
			{
				RegisterContainerToValidate(World, MapPath.GetPackageName(), ContainersToValidate);
			}
		}

		// Build a set of Relevant Actor Guids to scope error messages to what's contained in the CL 
		RelevantMap = MapPath;
		RelevantActorGuids.Reset();

		for (const FAssetData& ActorData : ActorsData)
		{
			// Get the FWorldPartitionActor			
			const FWorldPartitionActorDesc* ActorDesc = ContainersToValidate.GetActorDesc(ActorData.AssetName.ToString());

			if (ActorDesc != nullptr)
			{
				RelevantActorGuids.Add(ActorDesc->GetGuid());
			}
		}

		// Invoke static WorldPartition Validation from the ActorDescContainer
		UWorldPartition::FCheckForErrorsParams Params;
		Params.ErrorHandler = this;
		Params.bEnableStreaming = !ULevel::GetIsStreamingDisabledFromPackage(MapPath.GetPackageName());

		ContainersToValidate.ForEachActorDescContainer([&Params](const UActorDescContainer* ActorDescContainer)
		{
			for (FActorDescList::TConstIterator<> ActorDescIt(ActorDescContainer); ActorDescIt; ++ActorDescIt)
			{
				check(!Params.ActorGuidsToContainerMap.Contains(ActorDescIt->GetGuid()));
				Params.ActorGuidsToContainerMap.Add(ActorDescIt->GetGuid(), ActorDescContainer);
			}
		});

		ContainersToValidate.ForEachActorDescContainer([&Params](const UActorDescContainer* ActorDescContainer)
		{
			Params.ActorDescContainer = ActorDescContainer;
			UWorldPartition::CheckForErrors(Params);
		});
	}

	if (Errors->Num())
	{
		return EDataValidationResult::Invalid;
	}
		
	return EDataValidationResult::Valid;
	
}

bool UWorldPartitionChangelistValidator::Filter(const FWorldPartitionActorDescView& ActorDescView)
{
	if (RelevantActorGuids.Find(ActorDescView.GetGuid()))
	{
		return true;
	}

	if (!RelevantMap.IsNull())
	{
		FSoftObjectPath ActorPath = ActorDescView.GetActorSoftPath();
		return ActorPath.GetAssetPath() == RelevantMap;
	}

	return false;
}

bool UWorldPartitionChangelistValidator::Filter(const UDataLayerInstance* InDataLayerInstance)
{
	const UDataLayerInstanceWithAsset* DataLayerWithAsset = Cast<UDataLayerInstanceWithAsset>(InDataLayerInstance);
	return DataLayerWithAsset != nullptr && DataLayerWithAsset->GetAsset() != nullptr && RelevantDataLayerAssets.Contains(DataLayerWithAsset->GetAsset()->GetPathName());
}

void UWorldPartitionChangelistValidator::OnInvalidRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, FName GridName)
{
	if (Filter(ActorDescView))
	{
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.InvalidRuntimeGrid", "Actor {0} has an invalid runtime grid {1}"),
											FText::FromString(GetFullActorName(ActorDescView)), 
											FText::FromName(GridName));

		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnInvalidReference(const FWorldPartitionActorDescView& ActorDescView, const FGuid& ReferenceGuid, FWorldPartitionActorDescView* ReferenceActorDescView)
{
	if (Filter(ActorDescView))
	{
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.InvalidReference", "Actor {0} has an invalid reference to {1}"),
											FText::FromString(GetFullActorName(ActorDescView)), 
											FText::FromString(ReferenceActorDescView ? GetFullActorName(*ReferenceActorDescView) : ReferenceGuid.ToString()));

		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnInvalidReferenceGridPlacement(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	if (Filter(ActorDescView) || Filter(ReferenceActorDescView))
	{
		// Only report errors for non-spatially loaded actor referencing a spatially loaded actor
		if (!ActorDescView.GetIsSpatiallyLoaded())
		{
			const FString SpatiallyLoadedActor(TEXT("Spatially loaded actor"));
			const FString NonSpatiallyLoadedActor(TEXT("Non-spatially loaded loaded actor"));

			FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.InvalidReferenceGridPlacement", "{0} {1} is referencing {2} {3}."),
												FText::FromString(ActorDescView.GetIsSpatiallyLoaded() ? *SpatiallyLoadedActor : *NonSpatiallyLoadedActor),
												FText::FromString(GetFullActorName(ActorDescView)),
												FText::FromString(ReferenceActorDescView.GetIsSpatiallyLoaded() ? *SpatiallyLoadedActor : *NonSpatiallyLoadedActor),
												FText::FromString(GetFullActorName(ReferenceActorDescView)));

			Errors->Add(CurrentError);
		}
	}
}

void UWorldPartitionChangelistValidator::OnInvalidReferenceDataLayers(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{	
	if (Filter(ActorDescView) || Filter(ReferenceActorDescView))
	{
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.DataLayerError", "{0} is referencing {1} but both actors are using a different set of runtime data layers."),
											FText::FromString(GetFullActorName(ActorDescView)),
											FText::FromString(GetFullActorName(ReferenceActorDescView)));

		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnInvalidReferenceRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	if (Filter(ActorDescView) || Filter(ReferenceActorDescView))
	{
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.RuntimeGridError", "{0} is referencing {1} but both actors are using a different runtime grid."),
			FText::FromString(GetFullActorName(ActorDescView)),
			FText::FromString(GetFullActorName(ReferenceActorDescView)));

		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnInvalidReferenceLevelScriptStreamed(const FWorldPartitionActorDescView& ActorDescView)
{
	if (Filter(ActorDescView))
	{		
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.InvalidReferenceLevelScriptStreamed", "Level script blueprint references streamed actor {0}."),
											FText::FromString(GetFullActorName(ActorDescView)));
		
		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnInvalidReferenceLevelScriptDataLayers(const FWorldPartitionActorDescView& ActorDescView)
{
	if (Filter(ActorDescView))
	{
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.InvalidReferenceLevelScriptDataLayers", "Level script blueprint references streamed actor {0} with a non empty set of data layers."),
											FText::FromString(GetFullActorName(ActorDescView)));

		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnInvalidReferenceDataLayerAsset(const UDataLayerInstanceWithAsset* DataLayerInstance)
{
	if (SubmittingWorldDataLayers)
	{
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.InvalidDataLayerAsset", "Data layer {0} has no data layer asset."),
			FText::FromName(DataLayerInstance->GetDataLayerFName()));

		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnDataLayerHierarchyTypeMismatch(const UDataLayerInstance* DataLayerInstance, const UDataLayerInstance* Parent)
{
	if (Filter(DataLayerInstance)
		|| Filter(Parent)
		|| SubmittingWorldDataLayers)
	{
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.DataLayerHierarchyTypeMismatch", "Data layer {0} is of type {1} and its parent {2} is of type {3}."),
			FText::FromString(DataLayerInstance->GetDataLayerFullName()),
			UEnum::GetDisplayValueAsText(DataLayerInstance->GetType()),
			FText::FromString(Parent->GetDataLayerFullName()),
			UEnum::GetDisplayValueAsText(Parent->GetType()));
	
		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnDataLayerAssetConflict(const UDataLayerInstanceWithAsset* DataLayerInstance, const UDataLayerInstanceWithAsset* ConflictingDataLayerInstance)
{
	if (Filter(DataLayerInstance)
		|| Filter(ConflictingDataLayerInstance)
		|| SubmittingWorldDataLayers)
	{
		FText CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.DataLayerAssetConflict", "Data layer instance {0} and data layer instance {1} are both referencing data layer asset {2}."),
			FText::FromName(DataLayerInstance->GetDataLayerFName()),
			FText::FromName(ConflictingDataLayerInstance->GetDataLayerFName()),
			FText::FromString(DataLayerInstance->GetAsset()->GetFullName()));

		Errors->Add(CurrentError);
	}
}

void UWorldPartitionChangelistValidator::OnActorNeedsResave(const FWorldPartitionActorDescView& ActorDescView)
{
	// Changelist validation already ensures that dirty actors must be part of the changelist
}

void UWorldPartitionChangelistValidator::OnLevelInstanceInvalidWorldAsset(const FWorldPartitionActorDescView& ActorDescView, FName WorldAsset, ELevelInstanceInvalidReason Reason)
{
	if (Filter(ActorDescView))
	{
		FText CurrentError;

		switch (Reason)
		{
		case ELevelInstanceInvalidReason::WorldAssetNotFound:
			CurrentError = FText::Format(LOCTEXT("DataValidation.Changelist.WorldPartition.LevelInstanceInvalidWorldAsset", "Level instance {0} has an invalid world asset {1}."),
				FText::FromString(GetFullActorName(ActorDescView)), 
				FText::FromName(WorldAsset));
			Errors->Add(CurrentError);
			break;
		case ELevelInstanceInvalidReason::WorldAssetNotUsingExternalActors:
			// Not a validation error
			break;
		case ELevelInstanceInvalidReason::WorldAssetImcompatiblePartitioned:
			// Not a validation error
			break;
		case ELevelInstanceInvalidReason::WorldAssetHasInvalidContainer:
			// We cannot treat that error as a validation error as it's possible to validate changelists without loading the world
			break;
		};
	}
}

#undef LOCTEXT_NAMESPACE

