// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionStreamingGeneration.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Engine/LevelScriptBlueprint.h"
#include "ActorReferencesUtils.h"
#include "ReferenceCluster.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationNullErrorHandler.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationLogErrorHandler.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationMapCheckErrorHandler.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "HAL/FileManager.h"
#include "Algo/Transform.h"

#define LOCTEXT_NAMESPACE "WorldPartition"

static FAutoConsoleCommand DumpStreamingGenerationLog(
	TEXT("wp.Editor.DumpStreamingGenerationLog"),
	TEXT("Dump the streaming generation log."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World(); World && !World->IsGameWorld())
		{
			if (UWorldPartition* WorldPartition = World->GetWorldPartition())
			{
				WorldPartition->GenerateStreaming();
				WorldPartition->FlushStreaming();
			}
		}
	})
);

FActorDescViewMap::FActorDescViewMap()
{}

TArray<const FWorldPartitionActorDescView*> FActorDescViewMap::FindByExactNativeClass(UClass* InExactNativeClass) const
{
	check(InExactNativeClass->IsNative());
	const FName NativeClassName = InExactNativeClass->GetFName();
	TArray<const FWorldPartitionActorDescView*> Result;
	ActorDescViewsByClass.MultiFind(NativeClassName, Result);
	return Result;
}

FWorldPartitionActorDescView* FActorDescViewMap::Emplace(const FGuid& InGuid, const FWorldPartitionActorDescView& InActorDescView)
{
	FWorldPartitionActorDescView* NewActorDescView = ActorDescViewList.Emplace_GetRef(MakeUnique<FWorldPartitionActorDescView>(InActorDescView)).Get();
	
	const UClass* NativeClass = NewActorDescView->GetActorNativeClass();
	const FName NativeClassName = NativeClass->GetFName();

	ActorDescViewsByGuid.Emplace(InGuid, NewActorDescView);
	ActorDescViewsByClass.Add(NativeClassName, NewActorDescView);

	return NewActorDescView;
}

class FWorldPartitionStreamingGenerator
{
	class FStreamingGenerationContext : public IStreamingGenerationContext
	{
	public:
		FStreamingGenerationContext(const FWorldPartitionStreamingGenerator* StreamingGenerator, const UActorDescContainer* MainWorldContainer)
		{
			// Create the dataset required for IStreamingGenerationContext interface
			MainWorldActorSetContainerIndex = INDEX_NONE;
			ActorSetContainers.Empty(StreamingGenerator->ContainerDescriptorsMap.Num());

			TMap<const UActorDescContainer*, int32> ActorSetContainerMap;
			for (const auto& [ActorDescContainer, ContainerDescriptor] : StreamingGenerator->ContainerDescriptorsMap)
			{
				const int32 ContainerIndex = ActorSetContainers.AddDefaulted();
				ActorSetContainerMap.Add(ContainerDescriptor.Container, ContainerIndex);

				FActorSetContainer& ActorSetContainer = ActorSetContainers[ContainerIndex];
				ActorSetContainer.ActorDescViewMap = &ContainerDescriptor.ActorDescViewMap;
				ActorSetContainer.ActorDescContainer = ContainerDescriptor.Container;

				ActorSetContainer.ActorSets.Empty(ContainerDescriptor.Clusters.Num());
				for (const TArray<FGuid>& Cluster : ContainerDescriptor.Clusters)
				{
					FActorSet& ActorSet = ActorSetContainer.ActorSets.Emplace_GetRef();
					ActorSet.Actors = Cluster;
				}

				if (ContainerDescriptor.Container == MainWorldContainer)
				{
					check(MainWorldActorSetContainerIndex == INDEX_NONE);
					MainWorldActorSetContainerIndex = ContainerIndex;
				}
			}

			ActorSetInstances.Empty();
			for (const auto& [ContainerID, ContainerInstanceDescriptor] : StreamingGenerator->ContainerInstanceDescriptorsMap)
			{
				const FContainerDescriptor& ContainerDescriptor = StreamingGenerator->ContainerDescriptorsMap.FindChecked(ContainerInstanceDescriptor.Container);
				const FActorSetContainer& ActorSetContainer = ActorSetContainers[ActorSetContainerMap.FindChecked(ContainerInstanceDescriptor.Container)];

				for (const FActorSet& ActorSet : ActorSetContainer.ActorSets)
				{
					const FWorldPartitionActorDescView& ReferenceActorDescView = ActorSetContainer.ActorDescViewMap->FindByGuidChecked(ActorSet.Actors[0]);

					// Validate assumptions
					for (const FGuid& ActorGuid : ActorSet.Actors)
					{
						const FWorldPartitionActorDescView& ActorDescView = ActorSetContainer.ActorDescViewMap->FindByGuidChecked(ActorGuid);
						check(ActorDescView.GetRuntimeGrid() == ReferenceActorDescView.GetRuntimeGrid());
						check(ActorDescView.GetIsSpatiallyLoaded() == ReferenceActorDescView.GetIsSpatiallyLoaded());
						check(ActorDescView.GetContentBundleGuid() == ReferenceActorDescView.GetContentBundleGuid());
					}

					FActorSetInstance& ActorSetInstance = ActorSetInstances.Emplace_GetRef();
				
					ActorSetInstance.ContainerInstance = &ActorSetContainer;
					ActorSetInstance.ActorSet = &ActorSet;
					ActorSetInstance.ContainerID = ContainerInstanceDescriptor.ID;
					ActorSetInstance.Transform = ContainerInstanceDescriptor.Transform;

					// Since Content Bundles streaming generation happens in its own context, all actor set instances must have the same content bundle GUID for now, so Level Instances
					// placed inside a Content Bundle will propagate their Content Bundle GUID to child instances.
					ActorSetInstance.ContentBundleID = MainWorldContainer->GetContentBundleGuid();

					if (ContainerInstanceDescriptor.ID.IsMainContainer())
					{
						// Main container will get inherited properties from the actor descriptors
						ActorSetInstance.RuntimeGrid = ReferenceActorDescView.GetRuntimeGrid();
						ActorSetInstance.bIsSpatiallyLoaded = ReferenceActorDescView.GetIsSpatiallyLoaded();
						ActorSetInstance.DataLayers = UDataLayerSubsystem::GetRuntimeDataLayerInstances(ContainerDescriptor.Container->GetWorld(), ReferenceActorDescView.GetRuntimeDataLayers());
					}
					else
					{
						// Sub containers will inherit some properties from the parent actor descriptors
						ActorSetInstance.RuntimeGrid = ContainerInstanceDescriptor.RuntimeGrid;
						ActorSetInstance.bIsSpatiallyLoaded = ContainerInstanceDescriptor.bIsSpatiallyLoaded;

						TSet<FName> CombinedDataLayers = ContainerInstanceDescriptor.RuntimeDataLayers;
						CombinedDataLayers.Append(ReferenceActorDescView.GetRuntimeDataLayers());

						ActorSetInstance.DataLayers = UDataLayerSubsystem::GetRuntimeDataLayerInstances(ContainerDescriptor.Container->GetWorld(), CombinedDataLayers.Array());						
					}

					ActorSetInstance.Bounds.Init();
					for (const FGuid& ActorGuid : ActorSetInstance.ActorSet->Actors)
					{
						const FWorldPartitionActorDescView& ActorDescView = ActorSetContainer.ActorDescViewMap->FindByGuidChecked(ActorGuid);
						ActorSetInstance.Bounds += ActorDescView.GetBounds().TransformBy(ContainerInstanceDescriptor.Transform);
					}
				}
			}

			WorldBounds = StreamingGenerator->ContainerInstanceDescriptorsMap.FindChecked(FActorContainerID::GetMainContainerID()).Bounds;
		}

		virtual ~FStreamingGenerationContext()
		{}

		//~Begin IStreamingGenerationContext interface
		virtual FBox GetWorldBounds() const override
		{
			return WorldBounds;
		}

		virtual const FActorSetContainer* GetMainWorldContainer() const override
		{
			return ActorSetContainers.IsValidIndex(MainWorldActorSetContainerIndex) ? &ActorSetContainers[MainWorldActorSetContainerIndex] : nullptr;
		}

		virtual void ForEachActorSetInstance(TFunctionRef<void(const FActorSetInstance&)> Func) const
		{
			for (const FActorSetInstance& ActorSetInstance : ActorSetInstances)
			{
				Func(ActorSetInstance);
			}
		}
		//~End IStreamingGenerationContext interface};

	private:
		FBox WorldBounds;
		int32 MainWorldActorSetContainerIndex;
		TArray<FActorSetContainer> ActorSetContainers;
		TArray<FActorSetInstance> ActorSetInstances;
	};

	struct FContainerDescriptor
	{
		FContainerDescriptor()
			: Container(nullptr)
		{}

		const UActorDescContainer* Container;
		FActorDescViewMap ActorDescViewMap;
		TArray<FWorldPartitionActorDescView> ContainerInstanceViews;
		TArray<TArray<FGuid>> Clusters;
	};

	struct FContainerInstanceDescriptor
	{
		FContainerInstanceDescriptor()
			: Bounds(ForceInit)
			, Container(nullptr)
		{}

		FBox Bounds;
		FTransform Transform;
		const UActorDescContainer* Container;
		EContainerClusterMode ClusterMode;
		TSet<FName> RuntimeDataLayers;
		FName RuntimeGrid;
		bool bIsSpatiallyLoaded;
		FString OwnerName;
		FActorContainerID ID;
	};

	void ResolveRuntimeSpatiallyLoaded(FWorldPartitionActorDescView& ActorDescView)
	{
		if (!bEnableStreaming)
		{
			ActorDescView.SetForcedNonSpatiallyLoaded();
		}
	}

	void ResolveRuntimeDataLayers(FWorldPartitionActorDescView& ActorDescView, const FActorDescViewMap& ActorDescViewMap)
	{
		TArray<FName> RuntimeDataLayerInstanceNames;
		RuntimeDataLayerInstanceNames.Reserve(ActorDescView.GetDataLayers().Num());

		if (FDataLayerUtils::ResolveRuntimeDataLayerInstanceNames(ActorDescView, ActorDescViewMap, RuntimeDataLayerInstanceNames))
		{
			ActorDescView.SetRuntimeDataLayers(RuntimeDataLayerInstanceNames);
		}
	}

	void ResolveRuntimeReferences(FWorldPartitionActorDescView& ActorDescView, const FActorDescViewMap& ActorDescViewMap)
	{
		TArray<FGuid> RuntimeReferences;
		RuntimeReferences.Reserve(ActorDescView.GetReferences().Num());

		for (const FGuid& ReferenceGuid : ActorDescView.GetReferences())
		{
			if (const FWorldPartitionActorDescView* ReferenceDescView = ActorDescViewMap.FindByGuid(ReferenceGuid))
			{
				check(!ReferenceDescView->GetActorIsEditorOnly());
				RuntimeReferences.Add(ReferenceGuid);
			}
		}

		if (RuntimeReferences.Num() != ActorDescView.GetReferences().Num())
		{
			ActorDescView.SetRuntimeReferences(RuntimeReferences);
		}
	}

	void CreateActorDescViewMap(const UActorDescContainer* InContainer, FActorDescViewMap& OutActorDescViewMap, const FActorContainerID& InContainerID, TArray<FWorldPartitionActorDescView>& OutContainerInstances)
	{
		// Should we handle unsaved or newly created actors?
		const bool bHandleUnsavedActors = ModifiedActorsDescList && InContainerID.IsMainContainer();

		// Consider all actors of a /Temp/ container package as Unsaved because loading them from disk will fail (Outer world name mismatch)
		const bool bIsTempContainerPackage = FPackageName::IsTempPackage(InContainer->GetPackage()->GetName());
		
		// Test whether an actor is editor only. Will fallback to the actor descriptor only if the actor is not loaded
		auto IsActorEditorOnly = [](const FWorldPartitionActorDesc* ActorDesc, const FActorContainerID& ContainerID) -> bool
		{
			if (ActorDesc->IsRuntimeRelevant(ContainerID))
			{
				if (ActorDesc->IsLoaded())
				{
					return ActorDesc->GetActor()->IsEditorOnly();
				}
				else
				{
					return ActorDesc->GetActorIsEditorOnly();
				}
			}
			return true;
		};

		auto IsFilteredActorClass = [this](const FWorldPartitionActorDesc* ActorDesc)
		{
			for (UClass* FilteredClass : FilteredClasses)
			{
				if (ActorDesc->GetActorNativeClass()->IsChildOf(FilteredClass))
				{
					return true;
				}
			}
			return false;
		};

		// Create an actor descriptor view for the specified actor (modified or unsaved actors)
		auto GetModifiedActorDesc = [this](AActor* InActor, const UActorDescContainer* InContainer) -> FWorldPartitionActorDesc*
		{
			FWorldPartitionActorDesc* ModifiedActorDesc = ModifiedActorsDescList->AddActor(InActor);

			// Pretend that this actor descriptor belongs to the original container, even if it's not present. It's essentially a proxy
			// descriptor on top an existing one and at this point no code should require to access the container to resolve it anyways.
			ModifiedActorDesc->SetContainer(const_cast<UActorDescContainer*>(InContainer));

			return ModifiedActorDesc;
		};

		// Register the actor descriptor view
		auto RegisterActorDescView = [this, InContainer, &OutActorDescViewMap, &OutContainerInstances](const FGuid& ActorGuid, FWorldPartitionActorDescView& InActorDescView)
		{
			if (InActorDescView.IsContainerInstance())
			{
				OutContainerInstances.Add(InActorDescView);
			}
			else
			{
				OutActorDescViewMap.Emplace(ActorGuid, InActorDescView);
			}
		};
		
		TMap<FGuid, FGuid> ContainerGuidsRemap;
		for (FActorDescList::TConstIterator<> ActorDescIt(InContainer); ActorDescIt; ++ActorDescIt)
		{
			if (!IsActorEditorOnly(*ActorDescIt, InContainerID) && !IsFilteredActorClass(*ActorDescIt))
			{
				// Handle unsaved actors
				if (AActor* Actor = ActorDescIt->GetActor())
				{
					// Deleted actors
					if (!IsValid(Actor))
					{
						continue;
					}

					// Dirty actors
					if (bHandleUnsavedActors && (bIsTempContainerPackage || Actor->GetPackage()->IsDirty()))
					{
						// Dirty, unsaved actor for PIE
						FWorldPartitionActorDescView ModifiedActorDescView = GetModifiedActorDesc(Actor, InContainer);
						RegisterActorDescView(ActorDescIt->GetGuid(), ModifiedActorDescView);
						continue;
					}
				}

				// Non-dirty actor
				FWorldPartitionActorDescView ActorDescView(*ActorDescIt);
				RegisterActorDescView(ActorDescIt->GetGuid(), ActorDescView);
			}
		}

		// Append new unsaved actors for the persistent level
		if (bHandleUnsavedActors)
		{
			for (AActor* Actor : InContainer->GetWorld()->PersistentLevel->Actors)
			{
				if (IsValid(Actor) && Actor->IsPackageExternal() && Actor->IsMainPackageActor() && !Actor->IsEditorOnly() && (Actor->GetContentBundleGuid() == InContainer->GetContentBundleGuid()) && !InContainer->GetActorDesc(Actor->GetActorGuid()))
				{
					FWorldPartitionActorDescView ModifiedActorDescView = GetModifiedActorDesc(Actor, InContainer);
					RegisterActorDescView(Actor->GetActorGuid(), ModifiedActorDescView);
				}
			}
		}
	}

	void CreateActorDescriptorViewsRecursive(const UActorDescContainer* InContainer, const FTransform& InTransform, FName InRuntimeGrid, bool bInIsSpatiallyLoaded, const TSet<FName>& InRuntimeDataLayers, const FActorContainerID& InContainerID, const FActorContainerID& InParentContainerID, EContainerClusterMode InClusterMode, const TCHAR* InOwnerName)
	{
		// Create or resolve container descriptor
		FContainerDescriptor& ContainerDescriptor = ContainerDescriptorsMap.FindOrAdd(InContainer);

		if (!ContainerDescriptor.Container)
		{
			ContainerDescriptor.Container = InContainer;
		
			// Gather actor descriptor views for this container
			CreateActorDescViewMap(InContainer, ContainerDescriptor.ActorDescViewMap, InContainerID, ContainerDescriptor.ContainerInstanceViews);

			// Resolve actor descriptor views before validation
			ResolveContainerDescriptor(ContainerDescriptor);

			// Validate container, fixing anything illegal, etc.
			ValidateContainerDescriptor(ContainerDescriptor, InContainerID.IsMainContainer());

			// Update container, computing cluster, bounds, etc.
			UpdateContainerDescriptor(ContainerDescriptor);
		}

		// Inherited parent properties logic
		auto InheritParentContainerProperties = [](const FActorContainerID& InParentContainerID, const FWorldPartitionActorDescView& InParentActorDescView, TSet<FName>& InOutRuntimeDataLayers, FName& InOutRuntimeGrid, bool& bInOutIsSpatiallyLoaded)
		{
			// Runtime grid and spatially loaded flag are only inherited from the main world, since level instance don't support setting these values on actors
			if (InParentContainerID.IsMainContainer())
			{
				InOutRuntimeGrid = InParentActorDescView.GetRuntimeGrid();
				bInOutIsSpatiallyLoaded = InParentActorDescView.GetIsSpatiallyLoaded();
			}

			// Data layers are accumulated down the hierarchy chain, since level instances supports data layers assignation on actors
			InOutRuntimeDataLayers.Append(InParentActorDescView.GetRuntimeDataLayers());
		};

		// Parse actor containers
		TArray<FWorldPartitionActorDescView> ContainerInstanceViews = ContainerDescriptor.ContainerInstanceViews;
		for (const FWorldPartitionActorDescView& ContainerInstanceView : ContainerInstanceViews)
		{
			const UActorDescContainer* SubContainer;
			EContainerClusterMode SubClusterMode;
			FTransform SubTransform;

			if (!ContainerInstanceView.GetContainerInstance(SubContainer, SubTransform, SubClusterMode))
			{
				// Don't generate an error when validating changelist because container instances
				// won't be registered only because the world is not loaded.
				if (const bool bGenerateError = !bIsChangelistValidation)
				{
					//@todo_ow: make a specific error for missing container instance sublevel?
					ErrorHandler->OnInvalidReference(ContainerInstanceView, FGuid());
				}
				continue;
			}

			check(SubContainer);

			const FGuid ActorGuid = ContainerInstanceView.GetGuid();
			const FActorContainerID SubContainerID(InContainerID, ActorGuid);

			// Inherit parent container properties
			FName InheritedRuntimeGrid = InRuntimeGrid;
			bool bInheritedbIsSpatiallyLoaded = bInIsSpatiallyLoaded;
			TSet<FName> InheritedRuntimeDataLayers = InRuntimeDataLayers;
			InheritParentContainerProperties(InContainerID, ContainerInstanceView, InheritedRuntimeDataLayers, InheritedRuntimeGrid, bInheritedbIsSpatiallyLoaded);

			CreateActorDescriptorViewsRecursive(SubContainer, SubTransform * InTransform, InheritedRuntimeGrid, bInheritedbIsSpatiallyLoaded, InheritedRuntimeDataLayers, SubContainerID, InContainerID, SubClusterMode, *ContainerInstanceView.GetActorLabelOrName().ToString());
		}

		// Create container instance descriptor
		check(!ContainerInstanceDescriptorsMap.Contains(InContainerID));

		FContainerInstanceDescriptor& ContainerInstanceDescriptor = ContainerInstanceDescriptorsMap.Add(InContainerID);
		ContainerInstanceDescriptor.Container = InContainer;
		ContainerInstanceDescriptor.Transform = InTransform;
		ContainerInstanceDescriptor.ClusterMode = InClusterMode;
		ContainerInstanceDescriptor.RuntimeDataLayers = InRuntimeDataLayers;
		ContainerInstanceDescriptor.RuntimeGrid = InRuntimeGrid;
		ContainerInstanceDescriptor.bIsSpatiallyLoaded = bInIsSpatiallyLoaded;
		ContainerInstanceDescriptor.OwnerName = InOwnerName;
		ContainerInstanceDescriptor.ID = InContainerID;

		// Maintain containers hierarchy, bottom up
		if (InContainerID != InParentContainerID)
		{
			ContainersHierarchy.Add(InContainerID, InParentContainerID);
		}
	}

	/** 
	 * Creates the actor descriptor views for the specified container.
	 */
	void CreateActorContainers(const UActorDescContainer* InContainer)
	{
		CreateActorDescriptorViewsRecursive(InContainer, FTransform::Identity, NAME_None, false, TSet<FName>(), FActorContainerID(), FActorContainerID(), EContainerClusterMode::Partitioned, TEXT("MainContainer"));

		// Update container instances bounds
		for (auto& [ContainerID, ContainerInstanceDescriptor] : ContainerInstanceDescriptorsMap)
		{
			const FContainerDescriptor& ContainerDescriptor = ContainerDescriptorsMap.FindChecked(ContainerInstanceDescriptor.Container);

			// Lambdas can't capture structured bindings
			const FTransform& ContainerInstanceDescriptorTransform = ContainerInstanceDescriptor.Transform;
			FBox& ContainerInstanceDescriptorBounds = ContainerInstanceDescriptor.Bounds;			
			check(!ContainerInstanceDescriptorBounds.IsValid);

			ContainerDescriptor.ActorDescViewMap.ForEachActorDescView([&ContainerInstanceDescriptorBounds, &ContainerInstanceDescriptorTransform](const FWorldPartitionActorDescView& ActorDescView)
			{
				if (ActorDescView.GetIsSpatiallyLoaded())
				{
					ContainerInstanceDescriptorBounds += ActorDescView.GetBounds().TransformBy(ContainerInstanceDescriptorTransform);
				}
			});
		}

		// Update parent containers bounds, this relies on the fact that ContainersHierarchy is built bottom up
		for (auto& [ChildContainerID, ParentContainerID] : ContainersHierarchy)
		{
			const FContainerInstanceDescriptor& CurrentContainer = ContainerInstanceDescriptorsMap.FindChecked(ChildContainerID);
			FContainerInstanceDescriptor& ParentContainer = ContainerInstanceDescriptorsMap.FindChecked(ParentContainerID);
			ParentContainer.Bounds += CurrentContainer.Bounds;
		}
	}

	/** 
	 * Perform various validations on the container descriptor, and adjust it based on different requirements. This needs to happen before updating
	 * containers bounds because some actor descriptor views might change grid placement, etc.
	 */
	void ResolveContainerDescriptor(FContainerDescriptor& ContainerDescriptor)
	{
		auto ResolveActorDescView = [this, &ContainerDescriptor](FWorldPartitionActorDescView& ActorDescView)
		{
			ResolveRuntimeSpatiallyLoaded(ActorDescView);
			ResolveRuntimeDataLayers(ActorDescView, ContainerDescriptor.ActorDescViewMap);
			ResolveRuntimeReferences(ActorDescView, ContainerDescriptor.ActorDescViewMap);
		};

		ContainerDescriptor.ActorDescViewMap.ForEachActorDescView([this, &ResolveActorDescView](FWorldPartitionActorDescView& ActorDescView)
		{
			ResolveActorDescView(ActorDescView);
		});

		for (FWorldPartitionActorDescView& ContainerInstanceView : ContainerDescriptor.ContainerInstanceViews)
		{
			ResolveActorDescView(ContainerInstanceView);
		}
	}

	/** 
	 * Perform various validations on the container descriptor, and adjust it based on different requirements. This needs to happen before updating
	 * containers bounds because some actor descriptor views might change grid placement, etc.
	 */
	void ValidateContainerDescriptor(FContainerDescriptor& ContainerDescriptor, bool bIsMainContainer)
	{
		if (bIsMainContainer)
		{
			TArray<FGuid> LevelScriptReferences;
			if (UWorld* World = ContainerDescriptor.Container->GetWorld())
			{
				// Gather all references to external actors from the level script and make them always loaded
				if (ULevelScriptBlueprint* LevelScriptBlueprint = World->PersistentLevel->GetLevelScriptBlueprint(true))
				{
					TArray<AActor*> LevelScriptExternalActorReferences = ActorsReferencesUtils::GetExternalActorReferences(LevelScriptBlueprint);
					Algo::Transform(LevelScriptExternalActorReferences, LevelScriptReferences, [](const AActor* Actor) { return Actor->GetActorGuid(); });
				}

				// Validate data layers
				if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(World))
				{
					DataLayerSubsystem->ForEachDataLayer([this](const UDataLayerInstance* DataLayerInstance)
					{
						DataLayerInstance->Validate(ErrorHandler);
						return true;
					});
				}
			}
			else
			{
				ULevel::GetLevelScriptExternalActorsReferencesFromPackage(ContainerDescriptor.Container->GetContainerPackage(), LevelScriptReferences);
			}

			for (const FGuid& LevelScriptReferenceActorGuid : LevelScriptReferences)
			{
				if (FWorldPartitionActorDescView* ActorDescView = ContainerDescriptor.ActorDescViewMap.FindByGuid(LevelScriptReferenceActorGuid))
				{
					if (ActorDescView->GetIsSpatiallyLoaded())
					{
						ErrorHandler->OnInvalidReferenceLevelScriptStreamed(*ActorDescView);
						ActorDescView->SetForcedNonSpatiallyLoaded();
					}

					if (ActorDescView->GetRuntimeDataLayers().Num())
					{
						ErrorHandler->OnInvalidReferenceLevelScriptDataLayers(*ActorDescView);
						ActorDescView->SetInvalidDataLayers();
					}
				}
			}
		}

		// Route standard CheckForErrors calls which should not modify actor descriptors in any ways
		ContainerDescriptor.ActorDescViewMap.ForEachActorDescView([this](FWorldPartitionActorDescView& ActorDescView)
		{
			ActorDescView.CheckForErrors(ErrorHandler);
		});

		// Perform various adjustements based on validations and report errors
		//
		// The first validation pass is used to report errors, subsequent passes are used to make corrections to the FWorldPartitionActorDescView
		// Since the references can form cycles/long chains in the data fixes might need to be propagated in multiple passes.
		// 
		// This works because fixes are deterministic and always apply the same way to both Actors being modified, so there's no ordering issues possible
		int32 NbErrorsDetected = INDEX_NONE;
		for(uint32 NbValidationPasses = 0; NbErrorsDetected; NbValidationPasses++)
		{
			NbErrorsDetected = 0;

			ContainerDescriptor.ActorDescViewMap.ForEachActorDescView([this, &ContainerDescriptor, &NbErrorsDetected, &NbValidationPasses](FWorldPartitionActorDescView& ActorDescView)
			{
				// Validate data layers
				auto IsReferenceGridPlacementValid = [](const FWorldPartitionActorDescView& RefererActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
				{
					const bool bIsActorDescSpatiallyLoaded = RefererActorDescView.GetIsSpatiallyLoaded();
					const bool bIsActorDescRefSpatiallyLoaded = ReferenceActorDescView.GetIsSpatiallyLoaded();
					return bIsActorDescSpatiallyLoaded == bIsActorDescRefSpatiallyLoaded;
				};

				// Validate grid placement
				auto IsReferenceDataLayersValid = [](const FWorldPartitionActorDescView& RefererActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
				{
					if (RefererActorDescView.GetRuntimeDataLayers().Num() == ReferenceActorDescView.GetRuntimeDataLayers().Num())
					{
						const TSet<FName> RefererActorDescDataLayers(RefererActorDescView.GetRuntimeDataLayers());
						const TSet<FName> ReferenceActorDescDataLayers(ReferenceActorDescView.GetRuntimeDataLayers());

						return RefererActorDescDataLayers.Includes(ReferenceActorDescDataLayers);
					}

					return false;
				};

				// Validate runtime grid
				auto IsReferenceRuntimeGridValid = [](const FWorldPartitionActorDescView& RefererActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
				{
					return RefererActorDescView.GetRuntimeGrid() == ReferenceActorDescView.GetRuntimeGrid();
				};

				struct FActorReferenceInfo
				{
					FGuid ActorGuid;
					FWorldPartitionActorDescView* ActorDesc;
					FGuid ReferenceGuid;
					FWorldPartitionActorDescView* ReferenceActorDesc;
				};

				// Build references List
				TArray<FActorReferenceInfo> References;

				// Add normal actor references
				for (const FGuid& ReferenceGuid : ActorDescView.GetReferences())
				{
					if (ReferenceGuid != ActorDescView.GetParentActor()) // References to the parent are inversed in their handling 
					{
						// Filter out parent back references
						FWorldPartitionActorDescView* ReferenceActorDesc = ContainerDescriptor.ActorDescViewMap.FindByGuid(ReferenceGuid);
						if (ReferenceActorDesc && ReferenceActorDesc->GetParentActor() == ActorDescView.GetGuid())
						{
							continue;
						}

						References.Emplace(FActorReferenceInfo{ ActorDescView.GetGuid(), &ActorDescView, ReferenceGuid, ReferenceActorDesc });
					}
				}

				// Add attach reference for the topmost parent, this reference is inverted since we consider the top most existing 
				// parent to be refering to us, not the child to be referering the parent
				FGuid ParentGuid = ActorDescView.GetParentActor();
				FWorldPartitionActorDescView* TopParentDescView = nullptr;

				while (ParentGuid.IsValid())
				{
					FWorldPartitionActorDescView* ParentDescView = ContainerDescriptor.ActorDescViewMap.FindByGuid(ParentGuid);
					
					if (ParentDescView)
					{
						TopParentDescView = ParentDescView;
						ParentGuid = ParentDescView->GetParentActor();
					}
					else
					{
						// we had a guid but parent cannot be found, this will be a missing reference
						break; 
					}
				}

				if (TopParentDescView)
				{
					References.Emplace(FActorReferenceInfo{ TopParentDescView->GetGuid(), TopParentDescView, ActorDescView.GetGuid(), &ActorDescView });
				}

				if (ParentGuid.IsValid())
				{
					// In case of missing parent add a missing reference 
					References.Emplace(FActorReferenceInfo{ ActorDescView.GetGuid(), &ActorDescView, ParentGuid, nullptr });
				}

				for (FActorReferenceInfo& Info : References)
				{
					FWorldPartitionActorDescView* RefererActorDescView = Info.ActorDesc;
					FWorldPartitionActorDescView* ReferenceActorDescView = Info.ReferenceActorDesc;

					if (ReferenceActorDescView)
					{
						// Validate grid placement
						if (!IsReferenceGridPlacementValid(*RefererActorDescView, *ReferenceActorDescView))
						{
							if (!NbValidationPasses)
							{
								ErrorHandler->OnInvalidReferenceGridPlacement(*RefererActorDescView, *ReferenceActorDescView);									
							}
							else
							{
								RefererActorDescView->SetForcedNonSpatiallyLoaded();
								ReferenceActorDescView->SetForcedNonSpatiallyLoaded();
							}

							NbErrorsDetected++;
						}

						if (!IsReferenceDataLayersValid(*RefererActorDescView, *ReferenceActorDescView))
						{
							if (!NbValidationPasses)
							{
								ErrorHandler->OnInvalidReferenceDataLayers(*RefererActorDescView, *ReferenceActorDescView);									
							}
							else
							{
								RefererActorDescView->SetInvalidDataLayers();
								ReferenceActorDescView->SetInvalidDataLayers();
							}

							NbErrorsDetected++;
						}

						if (!IsReferenceRuntimeGridValid(*RefererActorDescView, *ReferenceActorDescView))
						{
							if (!NbValidationPasses)
							{
								ErrorHandler->OnInvalidReferenceRuntimeGrid(*RefererActorDescView, *ReferenceActorDescView);
							}
							else
							{
								RefererActorDescView->SetInvalidRuntimeGrid();
								ReferenceActorDescView->SetInvalidRuntimeGrid();
							}

							NbErrorsDetected++;
						}

					}
					else
					{
						if (!NbValidationPasses)
						{
							ErrorHandler->OnInvalidReference(*RefererActorDescView, Info.ReferenceGuid);
						}
						// Do not increment NbErrorsDetected since it won't be fixed and thus will always occur
					}
				}
			});		
		}
	}

	/** 
	 * Update the container descriptor containers to adjust their bounds from actor descriptor views.
	 */
	void UpdateContainerDescriptor(FContainerDescriptor& ContainerDescriptor)
	{
		// Build clusters for this container - at this point, all actors references should be in the same data layers, grid, etc because of actor descriptors validation.
		TArray<TPair<FGuid, TArray<FGuid>>> ActorsWithRefs;
		ContainerDescriptor.ActorDescViewMap.ForEachActorDescView([&ActorsWithRefs](const FWorldPartitionActorDescView& ActorDescView) { ActorsWithRefs.Emplace(ActorDescView.GetGuid(), ActorDescView.GetReferences()); });
		ContainerDescriptor.Clusters = GenerateObjectsClusters(ActorsWithRefs);
	}

public:
	FWorldPartitionStreamingGenerator(FActorDescList* InModifiedActorsDescList, IStreamingGenerationErrorHandler* InErrorHandler, bool bInEnableStreaming, bool bInIsChangelistValidation = false, TArray<TSubclassOf<AActor>> InFilteredClasses = {})
		: bEnableStreaming(bInEnableStreaming)
		, bIsChangelistValidation(bInIsChangelistValidation)
		, ModifiedActorsDescList(InModifiedActorsDescList)
		, FilteredClasses(InFilteredClasses)
		, ErrorHandler(InErrorHandler ? InErrorHandler : &NullErrorHandler)	
	{}

	void PreparationPhase(const UActorDescContainer* Container)
	{
		CreateActorContainers(Container);

		// Construct the streaming generation context
		StreamingGenerationContext = MakeUnique<FStreamingGenerationContext>(this, Container);
	}

	static TUniquePtr<FArchive> CreateDumpStateLogArchive(const TCHAR* Suffix)
	{
		const FString StateLogOutputFilename = FPaths::ProjectSavedDir() / TEXT("WorldPartition") / FString::Printf(TEXT("StreamingGeneration-%s-%08x-%s.log"), Suffix, FPlatformProcess::GetCurrentProcessId(), *FDateTime::Now().ToString());
		return TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*StateLogOutputFilename));
	}

	void DumpStateLog(FHierarchicalLogArchive& Ar)
	{
		// Build the containers tree representation
		TMultiMap<FActorContainerID, FActorContainerID> InvertedContainersHierarchy;
		for (auto& [ChildContainerID, ParentContainerID] : ContainersHierarchy)
		{
			InvertedContainersHierarchy.Add(ParentContainerID, ChildContainerID);
		}

		UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("Containers:")));
		for (auto& [ActorDescContainer, ContainerDescriptor] : ContainerDescriptorsMap)
		{
			UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("Container: %s"), *ContainerDescriptor.Container->GetContainerPackage().ToString()));

			if (ContainerDescriptor.ActorDescViewMap.Num())
			{
				UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("ActorDescs:")));

				TMap<FGuid, FWorldPartitionActorDescView*> SortedActorDescViewMap = ContainerDescriptor.ActorDescViewMap.ActorDescViewsByGuid;
				SortedActorDescViewMap.KeySort([](const FGuid& GuidA, const FGuid& GuidB) { return GuidA < GuidB; });

				for (auto& [ActorGuid, ActorDescView] : SortedActorDescViewMap)
				{
					Ar.Print(*ActorDescView->ToString());
				}
			}

			if (ContainerDescriptor.Clusters.Num())
			{
				UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("Clusters:")));

				int ClusterIndex = 0;
				for (TArray<FGuid>& ActorGuids : ContainerDescriptor.Clusters)
				{
					UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("[%3d]"), ClusterIndex++));
					for (const FGuid& ActorGuid : ActorGuids)
					{
						const FWorldPartitionActorDescView& ActorDescView = ContainerDescriptor.ActorDescViewMap.FindByGuidChecked(ActorGuid);
						Ar.Print(*ActorDescView.ToString());
					}
				}
			}
		}

		Ar.Printf(TEXT("ContainerInstances:"));
		auto DumpContainerInstances = [this, &InvertedContainersHierarchy, &Ar](const FActorContainerID& ContainerID)
		{
			auto DumpContainerInstancesRecursive = [this, &InvertedContainersHierarchy, &Ar](const FActorContainerID& ContainerID, auto& RecursiveFunc) -> void
			{
				const FContainerInstanceDescriptor& ContainerInstanceDescriptor = ContainerInstanceDescriptorsMap.FindChecked(ContainerID);
				
				{
					UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("%s:"), *ContainerInstanceDescriptor.OwnerName));

					Ar.Printf(TEXT("       ID: 0x%016llx"), ContainerID.ID);
					Ar.Printf(TEXT("   Bounds: %s"), *ContainerInstanceDescriptor.Bounds.ToString());
					Ar.Printf(TEXT("Transform: %s"), *ContainerInstanceDescriptor.Transform.ToString());
					Ar.Printf(TEXT("Container: %s"), *ContainerInstanceDescriptor.Container->GetContainerPackage().ToString());
				}

				TArray<FActorContainerID> ChildContainersIDs;
				InvertedContainersHierarchy.MultiFind(ContainerID, ChildContainersIDs);
				ChildContainersIDs.Sort([](const FActorContainerID& ActorContainerIDA, const FActorContainerID& ActorContainerIDB) { return ActorContainerIDA.ID < ActorContainerIDB.ID; });

				if (ChildContainersIDs.Num())
				{
					UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("SubContainers:")));
						
					for (const FActorContainerID& ChildContainerID : ChildContainersIDs)
					{
						RecursiveFunc(ChildContainerID, RecursiveFunc);
					}
				}
			};

			DumpContainerInstancesRecursive(ContainerID, DumpContainerInstancesRecursive);
		};

		DumpContainerInstances(FActorContainerID());
	}

	const FStreamingGenerationContext* GetStreamingGenerationContext()
	{
		return StreamingGenerationContext.Get();
	}

private:
	bool bEnableStreaming;
	bool bIsChangelistValidation;
	FActorDescList* ModifiedActorsDescList;
	TArray<TSubclassOf<AActor>> FilteredClasses;
	IStreamingGenerationErrorHandler* ErrorHandler;
	FStreamingGenerationNullErrorHandler NullErrorHandler;

	/** Maps containers to their container descriptor */
	TMap<const UActorDescContainer*, FContainerDescriptor> ContainerDescriptorsMap;
	
	/** Maps containers IDs to their container instance descriptor */
	TMap<FActorContainerID, FContainerInstanceDescriptor> ContainerInstanceDescriptorsMap;

	/** Maps containers IDs to their parent ID */
	TMap<FActorContainerID, FActorContainerID> ContainersHierarchy;

	/** Data required for streaming generation interface */
	TUniquePtr<FStreamingGenerationContext> StreamingGenerationContext;
};

bool UWorldPartition::GenerateStreaming(TArray<FString>* OutPackagesToGenerate)
{
	OnPreGenerateStreaming.Broadcast(OutPackagesToGenerate);

	return GenerateContainerStreaming(ActorDescContainer, OutPackagesToGenerate);
}

bool UWorldPartition::GenerateContainerStreaming(const UActorDescContainer* InActorDescContainer, TArray<FString>* OutPackagesToGenerate /* = nullptr */)
{
	FActorDescList* ModifiedActorsDescList = nullptr;

	FStreamingGenerationLogErrorHandler LogErrorHandler;
	FStreamingGenerationMapCheckErrorHandler MapCheckErrorHandler;	
	IStreamingGenerationErrorHandler* ErrorHandler = &LogErrorHandler;	

	if (bIsPIE)
	{
		ModifiedActorsDescList = &RuntimeHash->ModifiedActorDescListForPIE;
		
		// In PIE, we always want to populate the map check dialog
		ErrorHandler = &MapCheckErrorHandler;
	}

	// Dump state log
	const TCHAR* StateLogSuffix = bIsPIE ? TEXT("PIE") : (IsRunningGame() ? TEXT("Game") : (IsRunningCookCommandlet() ? TEXT("Cook") : TEXT("Manual")));
	TUniquePtr<FArchive> LogFileAr = FWorldPartitionStreamingGenerator::CreateDumpStateLogArchive(StateLogSuffix);
	FHierarchicalLogArchive HierarchicalLogAr(*LogFileAr);

	FWorldPartitionStreamingGenerator StreamingGenerator(ModifiedActorsDescList, ErrorHandler, IsStreamingEnabled());

	// Preparation Phase
	StreamingGenerator.PreparationPhase(InActorDescContainer);

	StreamingGenerator.DumpStateLog(HierarchicalLogAr);

	// Generate streaming
	check(!StreamingPolicy);
	StreamingPolicy = NewObject<UWorldPartitionStreamingPolicy>(const_cast<UWorldPartition*>(this), WorldPartitionStreamingPolicyClass.Get(), NAME_None, bIsPIE ? RF_Transient : RF_NoFlags);

	check(RuntimeHash);
	if (RuntimeHash->GenerateStreaming(StreamingPolicy, StreamingGenerator.GetStreamingGenerationContext(), OutPackagesToGenerate))
	{
		//if (IsRunningCookCommandlet())
		{
			RuntimeHash->DumpStateLog(HierarchicalLogAr);
		}

		StreamingPolicy->PrepareActorToCellRemapping();
		return true;
	}

	return false;
}

void UWorldPartition::FlushStreaming()
{
	RuntimeHash->FlushStreaming();
	StreamingPolicy = nullptr;
	GeneratedStreamingPackageNames.Empty();
}

void UWorldPartition::GenerateHLOD(ISourceControlHelper* SourceControlHelper, bool bCreateActorsOnly)
{
	FStreamingGenerationLogErrorHandler LogErrorHandler;
	const bool bInIsChangelistValidation = false;
	FWorldPartitionStreamingGenerator StreamingGenerator(nullptr, &LogErrorHandler, IsStreamingEnabled(), bInIsChangelistValidation, { AWorldPartitionHLOD::StaticClass() });
	StreamingGenerator.PreparationPhase(ActorDescContainer);

	TUniquePtr<FArchive> LogFileAr = FWorldPartitionStreamingGenerator::CreateDumpStateLogArchive(TEXT("HLOD"));
	FHierarchicalLogArchive HierarchicalLogAr(*LogFileAr);
	StreamingGenerator.DumpStateLog(HierarchicalLogAr);

	RuntimeHash->GenerateHLOD(SourceControlHelper, StreamingGenerator.GetStreamingGenerationContext(), bCreateActorsOnly);
}

void UWorldPartition::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	const bool bIsChangelistValidation = false;
	CheckForErrors(ErrorHandler, ActorDescContainer, IsStreamingEnabled(), bIsChangelistValidation);
}

void UWorldPartition::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler, const UActorDescContainer* ActorDescContainer, bool bEnableStreaming, bool bIsChangelistValidation)
{
	FActorDescList ModifiedActorDescList;
	FWorldPartitionStreamingGenerator StreamingGenerator(ActorDescContainer->GetWorld() ? &ModifiedActorDescList : nullptr, ErrorHandler, bEnableStreaming, bIsChangelistValidation);
	StreamingGenerator.PreparationPhase(ActorDescContainer);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
