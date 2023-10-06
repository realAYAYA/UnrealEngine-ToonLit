// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionStreamingGeneration.h"
#include "Misc/HierarchicalLogArchive.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"

#include "Editor.h"
#include "Algo/ForEach.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "Algo/Unique.h"
#include "Containers/ArrayView.h"
#include "Engine/LevelScriptBlueprint.h"
#include "ActorReferencesUtils.h"
#include "Misc/PackageName.h"
#include "ReferenceCluster.h"
#include "Misc/HashBuilder.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartition/WorldPartitionLevelStreamingPolicy.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationNullErrorHandler.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationLogErrorHandler.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationMapCheckErrorHandler.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "HAL/FileManager.h"

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
				UWorldPartition::FGenerateStreamingParams Params;
				UWorldPartition::FGenerateStreamingContext Context;
				WorldPartition->GenerateStreaming(Params, Context);
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

UWorldPartition::FCheckForErrorsParams::FCheckForErrorsParams()
	: ErrorHandler(nullptr)
	, ActorDescContainer(nullptr)
	, bEnableStreaming(false)
{}

class FWorldPartitionStreamingGenerator
{
	class FStreamingGenerationContext : public IStreamingGenerationContext
	{
	public:
		FStreamingGenerationContext(const FWorldPartitionStreamingGenerator* StreamingGenerator, const FStreamingGenerationActorDescCollection& TopLevelActorDescCollection)
		{
			// Create the dataset required for IStreamingGenerationContext interface
			MainWorldActorSetContainerIndex = INDEX_NONE;
			ActorSetContainers.Empty(StreamingGenerator->ContainerCollectionDescriptorsMap.Num());

			TMap<TWeakPtr<FStreamingGenerationActorDescCollection>, int32> ActorSetContainerMap;
			for (const auto& [LevelName, ContainerDescriptor] : StreamingGenerator->ContainerCollectionDescriptorsMap)
			{
				const int32 ContainerIndex = ActorSetContainers.AddDefaulted();
				ActorSetContainerMap.Add(ContainerDescriptor.ActorDescCollection, ContainerIndex);

				FActorSetContainer& ActorSetContainer = ActorSetContainers[ContainerIndex];
				ActorSetContainer.ActorDescViewMap = &ContainerDescriptor.ActorDescViewMap;
				ActorSetContainer.ActorDescCollection = ContainerDescriptor.ActorDescCollection.Get();

				ActorSetContainer.ActorSets.Empty(ContainerDescriptor.Clusters.Num());
				for (const TArray<FGuid>& Cluster : ContainerDescriptor.Clusters)
				{
					FActorSet& ActorSet = *ActorSetContainer.ActorSets.Add_GetRef(MakeUnique<FActorSet>()).Get();
					ActorSet.Actors = Cluster;
				}

				if (ContainerDescriptor.ActorDescCollection->GetMainContainerPackageName() == TopLevelActorDescCollection.GetMainContainerPackageName())
				{
					check(MainWorldActorSetContainerIndex == INDEX_NONE);
					MainWorldActorSetContainerIndex = ContainerIndex;
				}
			}

			ActorSetInstances.Empty();
			for (const auto& [ContainerID, ContainerCollectionInstanceDescriptor] : StreamingGenerator->ContainerCollectionInstanceDescriptorsMap)
			{
				const FContainerCollectionDescriptor& ContainerCollectionDescriptor = StreamingGenerator->ContainerCollectionDescriptorsMap.FindChecked(ContainerCollectionInstanceDescriptor.ActorDescCollection->GetMainContainerPackageName());
				const FActorSetContainer& ActorSetContainer = ActorSetContainers[ActorSetContainerMap.FindChecked(ContainerCollectionDescriptor.ActorDescCollection)];
				const TSet<FGuid>* FilteredActors = StreamingGenerator->ContainerFilteredActors.Find(ContainerID);
				for (const TUniquePtr<FActorSet>& ActorSetPtr : ActorSetContainer.ActorSets)
				{
					const FActorSet& ActorSet = *ActorSetPtr;
					const FWorldPartitionActorDescView& ReferenceActorDescView = ActorSetContainer.ActorDescViewMap->FindByGuidChecked(ActorSet.Actors[0]);

					bool bContainsUnfilteredActors = !FilteredActors;
					// Validate assumptions
					for (const FGuid& ActorGuid : ActorSet.Actors)
					{
						const FWorldPartitionActorDescView& ActorDescView = ActorSetContainer.ActorDescViewMap->FindByGuidChecked(ActorGuid);
						check(ActorDescView.GetRuntimeGrid() == ReferenceActorDescView.GetRuntimeGrid());
						check(ActorDescView.GetIsSpatiallyLoaded() == ReferenceActorDescView.GetIsSpatiallyLoaded());
						check(ActorDescView.GetContentBundleGuid() == ReferenceActorDescView.GetContentBundleGuid());
						bContainsUnfilteredActors |= (FilteredActors && !FilteredActors->Contains(ActorGuid));
					}

					// Skip if all actors are filtered out for this container
					if (bContainsUnfilteredActors)
					{
						FActorSetInstance& ActorSetInstance = ActorSetInstances.Emplace_GetRef();
						const FContainerCollectionInstanceDescriptor::FPerInstanceData& PerInstanceData = ContainerCollectionInstanceDescriptor.GetInstanceData(ReferenceActorDescView.GetGuid());
				
						ActorSetInstance.ContainerInstance = &ActorSetContainer;
						ActorSetInstance.ActorSet = &ActorSet;
						ActorSetInstance.FilteredActors = FilteredActors;
						ActorSetInstance.ContainerID = ContainerCollectionInstanceDescriptor.ID;
						ActorSetInstance.Transform = ContainerCollectionInstanceDescriptor.Transform;
						ActorSetInstance.bIsSpatiallyLoaded = PerInstanceData.bIsSpatiallyLoaded;
						ActorSetInstance.ContentBundleID = ContainerCollectionInstanceDescriptor.ContentBundleID;
						ActorSetInstance.RuntimeGrid = PerInstanceData.RuntimeGrid;
						ActorSetInstance.DataLayers = StreamingGenerator->GetRuntimeDataLayerInstances(PerInstanceData.DataLayers);

						ActorSetInstance.Bounds.Init();
						const FTransform& ContainerCollectionInstanceDescriptorTransform = ContainerCollectionInstanceDescriptor.Transform;
						ActorSetInstance.ForEachActor([this, &ActorSetContainer, &ActorSetInstance, &ContainerCollectionInstanceDescriptorTransform](const FGuid& ActorGuid)
						{
							const FWorldPartitionActorDescView& ActorDescView = ActorSetContainer.ActorDescViewMap->FindByGuidChecked(ActorGuid);
							const FBox RuntimeBounds = ActorDescView.GetRuntimeBounds();
							if (RuntimeBounds.IsValid)
							{
								ActorSetInstance.Bounds += RuntimeBounds.TransformBy(ContainerCollectionInstanceDescriptorTransform);
							}
						});
					}
				}
			}

			WorldBounds = StreamingGenerator->ContainerCollectionInstanceDescriptorsMap.FindChecked(FActorContainerID::GetMainContainerID()).Bounds;
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

		virtual void ForEachActorSetInstance(TFunctionRef<void(const FActorSetInstance&)> Func) const override
		{
			for (const FActorSetInstance& ActorSetInstance : ActorSetInstances)
			{
				Func(ActorSetInstance);
			}
		}

		virtual void ForEachActorSetContainer(TFunctionRef<void(const FActorSetContainer&)> Func) const override
		{
			for (const FActorSetContainer& ActorSetContainer : ActorSetContainers)
			{
				Func(ActorSetContainer);
			}
		}
		//~End IStreamingGenerationContext interface};

	private:
		FBox WorldBounds;
		int32 MainWorldActorSetContainerIndex;
		TArray<FActorSetContainer> ActorSetContainers;
		TArray<FActorSetInstance> ActorSetInstances;
	};

	/** 
	 * An actor container descriptor, one for the main world and one for every unique level used by level instances.
	 */
	struct FContainerCollectionDescriptor
	{
		/** The unique actor descriptor container collection for this descriptor */
		TSharedPtr<FStreamingGenerationActorDescCollection> ActorDescCollection;

		/** The actor descriptor views for for this descriptor */
		FActorDescViewMap ActorDescViewMap;

		/** Set of editor-only actors that are not part of the actor descriptor views */
		TSet<FGuid> EditorOnlyActorDescMap;

		/** List of actor descriptor views that are containers (mainly level instances) */
		TArray<FWorldPartitionActorDescView> ContainerCollectionInstanceViews;

		/** List of actor clusters for this descriptor */
		TArray<TArray<FGuid>> Clusters;
	};

	/** 
	 * An actor container instance descriptor, one for the main world and one for every actor container instance
	 */
	struct FContainerCollectionInstanceDescriptor
	{
		FContainerCollectionInstanceDescriptor()
			: Bounds(ForceInit)
		{}

		FBox Bounds;
		FTransform Transform;
		TSharedPtr<FStreamingGenerationActorDescCollection> ActorDescCollection;
		EContainerClusterMode ClusterMode;
		FString OwnerName;
		FActorContainerID ID;
		FActorContainerID ParentID;
		FGuid ContentBundleID;

		struct FPerInstanceData
		{
			bool bIsSpatiallyLoaded;
			FName RuntimeGrid;
			TArray<FName> DataLayers;

			inline bool operator==(const FPerInstanceData& Other) const
			{
				// Assumes DataLayers are sorted
				return 
					(bIsSpatiallyLoaded == Other.bIsSpatiallyLoaded) && 
					(RuntimeGrid == Other.RuntimeGrid) && 
					(DataLayers == Other.DataLayers);
			}

			inline bool operator!=(const FPerInstanceData& Other) const
			{
				return !operator==(Other);
			}

			friend inline uint32 GetTypeHash(const FPerInstanceData& InPerInstanceData)
			{
				FHashBuilder HashBuilder;
				HashBuilder << InPerInstanceData.bIsSpatiallyLoaded << InPerInstanceData.RuntimeGrid << InPerInstanceData.DataLayers;
				return HashBuilder.GetHash();
			}
		};

		FPerInstanceData& GetInstanceData(const FGuid& ActorGuid)
		{
			if (const FSetElementId* PerInstanceDataId = PerInstanceData.Find(ActorGuid))
			{
				check(UniquePerInstanceData.IsValidId(*PerInstanceDataId));
				return UniquePerInstanceData[*PerInstanceDataId];
			}

			return InstanceData;
		}

		const FPerInstanceData& GetInstanceData(const FGuid& ActorGuid) const
		{
			if (const FSetElementId* PerInstanceDataId = PerInstanceData.Find(ActorGuid))
			{
				check(UniquePerInstanceData.IsValidId(*PerInstanceDataId));
				return UniquePerInstanceData[*PerInstanceDataId];
			}

			return InstanceData;
		}

		FPerInstanceData InstanceData;
		TSet<FPerInstanceData> UniquePerInstanceData;
		TMap<FGuid, FSetElementId> PerInstanceData;
	};

	void ResolveRuntimeSpatiallyLoaded(FWorldPartitionActorDescView& ActorDescView)
	{
		if (!bEnableStreaming)
		{
			ActorDescView.SetForcedNonSpatiallyLoaded();
		}
	}

	void ResolveRuntimeGrid(FWorldPartitionActorDescView& ActorDescView)
	{
		if (!bEnableStreaming)
		{
			ActorDescView.SetForcedNoRuntimeGrid();
		}
	}

	void ResolveRuntimeDataLayers(FWorldPartitionActorDescView& ActorDescView, const FActorDescViewMap& ActorDescViewMap)
	{
		// Resolve DataLayerInstanceNames of ActorDescView only when necessary (i.e. when container is a template)
		if (!ActorDescView.GetActorDesc()->HasResolvedDataLayerInstanceNames())
		{
			// Build a WorldDataLayerActorDescs if DataLayerManager can't resolve Data Layers (i.e. when validating changelists and World is not loaded)
			const bool bDataLayerManagerCanResolve = DataLayerManager && DataLayerManager->CanResolveDataLayers();
			const TArray<const FWorldDataLayersActorDesc*> WorldDataLayerActorDescs = !bDataLayerManagerCanResolve ? FDataLayerUtils::FindWorldDataLayerActorDescs(ActorDescViewMap) : TArray<const FWorldDataLayersActorDesc*>();
			const TArray<FName> DataLayerInstanceNames = FDataLayerUtils::ResolvedDataLayerInstanceNames(DataLayerManager, ActorDescView.GetActorDesc(), WorldDataLayerActorDescs);
			ActorDescView.SetDataLayerInstanceNames(DataLayerInstanceNames);
		}

		TArray<FName> RuntimeDataLayerInstanceNames;
		if (FDataLayerUtils::ResolveRuntimeDataLayerInstanceNames(DataLayerManager, ActorDescView, ActorDescViewMap, RuntimeDataLayerInstanceNames))
		{
			ActorDescView.SetRuntimeDataLayerInstanceNames(RuntimeDataLayerInstanceNames);
		}
	}

	void CreateActorDescViewMap(const FStreamingGenerationActorDescCollection& InActorDescCollection, FActorDescViewMap& OutActorDescViewMap, TSet<FGuid>& OutEditorOnlyActorDescMap, const FActorContainerID& InContainerID, TArray<FWorldPartitionActorDescView>& OutContainerInstances)
	{
		// Should we handle unsaved or newly created actors?
		const bool bHandleUnsavedActors = ModifiedActorsDescList && InContainerID.IsMainContainer();

		// Consider all actors of a /Temp/ container package as Unsaved because loading them from disk will fail (Outer world name mismatch)
		const bool bIsTempContainerPackage = FPackageName::IsTempPackage((InActorDescCollection.GetMainContainerPackageName().ToString()));

		// Create an actor descriptor view for the specified actor (modified or unsaved actors)
		auto GetModifiedActorDesc = [this](AActor* InActor, const FStreamingGenerationActorDescCollection& InActorDescCollection) -> FWorldPartitionActorDesc*
		{
			FWorldPartitionActorDesc* ModifiedActorDesc = ModifiedActorsDescList->AddActor(InActor);

			const UActorDescContainer* HandlingContainer = InActorDescCollection.FindHandlingContainer(InActor);
			if (HandlingContainer == nullptr)
			{
				// @todo_ow : UActorDescContainer::IsActorDescHandled(AActor*) does not handle the case where the InActor is an unsaved new non-plugin actor
				// and the level is in memory. Fix UActorDescContainer::IsActorDescHandled so it handle those actors
				HandlingContainer = InActorDescCollection.GetMainActorDescContainer();
			}

			check(HandlingContainer != nullptr);

			// Pretend that this actor descriptor belongs to the original container, even if it's not present. It's essentially a proxy
			// descriptor on top an existing one and at this point no code should require to access the container to resolve it anyways.
			ModifiedActorDesc->SetContainer(const_cast<UActorDescContainer*>(HandlingContainer), InActor->GetWorld());

			return ModifiedActorDesc;
		};

		// Test whether an actor should be included in the ActorDescViewMap.
		auto ShouldRegisterActorDesc = [this](const FWorldPartitionActorDesc* ActorDesc, const FActorContainerID& ContainerID)
		{
			for (UClass* FilteredClass : FilteredClasses)
			{
				if (ActorDesc->GetActorNativeClass()->IsChildOf(FilteredClass))
				{
					return false;
				}
			}

			if (!ActorDesc->IsRuntimeRelevant(ContainerID))
			{
				return false;
			}

			return ActorDesc->IsLoaded() ? !ActorDesc->GetActor()->IsEditorOnly() : !ActorDesc->GetActorIsEditorOnly();
		};

		// Register the actor descriptor view
		auto RegisterActorDescView = [this, &OutActorDescViewMap, &OutContainerInstances](const FGuid& ActorGuid, FWorldPartitionActorDescView& InActorDescView)
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
		for (FStreamingGenerationActorDescCollection::TConstIterator<> ActorDescIt(&InActorDescCollection); ActorDescIt; ++ActorDescIt)
		{
			if (ShouldRegisterActorDesc(*ActorDescIt, InContainerID))
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
						FWorldPartitionActorDescView ModifiedActorDescView = GetModifiedActorDesc(Actor, InActorDescCollection);
						RegisterActorDescView(ActorDescIt->GetGuid(), ModifiedActorDescView);
						continue;
					}
				}

				// Non-dirty actor
				FWorldPartitionActorDescView ActorDescView(*ActorDescIt);
				RegisterActorDescView(ActorDescIt->GetGuid(), ActorDescView);
			}
			else
			{
				OutEditorOnlyActorDescMap.Add(ActorDescIt->GetGuid());
			}
		}

		// Append new unsaved actors for the persistent level
		if (bHandleUnsavedActors)
		{
			for (AActor* Actor : InActorDescCollection.GetWorld()->PersistentLevel->Actors)
			{
				if (IsValid(Actor) && Actor->IsPackageExternal() && Actor->IsMainPackageActor() && !Actor->IsEditorOnly()
					&& (Actor->GetContentBundleGuid() == InActorDescCollection.GetContentBundleGuid())
					&& !InActorDescCollection.GetActorDesc(Actor->GetActorGuid()))
				{
					FWorldPartitionActorDescView ModifiedActorDescView = GetModifiedActorDesc(Actor, InActorDescCollection);
					if (ShouldRegisterActorDesc(ModifiedActorDescView.GetActorDesc(), InContainerID))
					{
						RegisterActorDescView(Actor->GetActorGuid(), ModifiedActorDescView);
					}
				}
			}
		}
	}

	void CreateActorDescriptorViewsRecursive(const FContainerCollectionInstanceDescriptor& InContainerCollectionInstanceDescriptor)
	{
		// Inherited parent per-instance data logic
		auto InheritParentContainerPerInstanceData = [&InContainerCollectionInstanceDescriptor](const FContainerCollectionInstanceDescriptor::FPerInstanceData& InParentPerInstanceData, const FWorldPartitionActorDescView& InActorDescView)
		{
			FContainerCollectionInstanceDescriptor::FPerInstanceData ResultPerInstanceData;

			// Apply AND logic on spatially loaded flag
			ResultPerInstanceData.bIsSpatiallyLoaded = InActorDescView.GetIsSpatiallyLoaded() && InParentPerInstanceData.bIsSpatiallyLoaded;

			// Runtime grid is only inherited from the main world, since level instance doesn't support setting this value on actors
			ResultPerInstanceData.RuntimeGrid = InContainerCollectionInstanceDescriptor.ID.IsMainContainer() ? InActorDescView.GetRuntimeGrid() : InParentPerInstanceData.RuntimeGrid;

			// Data layers are accumulated down the hierarchy chain, since level instances supports data layers assignation on actors
			ResultPerInstanceData.DataLayers = InActorDescView.GetRuntimeDataLayerInstanceNames();
			ResultPerInstanceData.DataLayers.Append(InParentPerInstanceData.DataLayers);
			ResultPerInstanceData.DataLayers.Sort(FNameFastLess());

			if (InParentPerInstanceData.DataLayers.Num())
			{
				// Remove potential duplicates from sorted data layers array
				ResultPerInstanceData.DataLayers.SetNum(Algo::Unique(ResultPerInstanceData.DataLayers));
			}

			return ResultPerInstanceData;
		};

		// ContainerDescriptor may be reallocated after this scope
		{
			// Create or resolve container descriptor
			FContainerCollectionDescriptor& ContainerCollectionDescriptor = ContainerCollectionDescriptorsMap.FindOrAdd(InContainerCollectionInstanceDescriptor.ActorDescCollection->GetMainContainerPackageName());
		
			// ContainerInstanceDescriptor may be reallocated after this scope
			{
				// Create container instance descriptor
				check(!ContainerCollectionInstanceDescriptorsMap.Contains(InContainerCollectionInstanceDescriptor.ID));

				FContainerCollectionInstanceDescriptor& ContainerCollectionInstanceDescriptor = ContainerCollectionInstanceDescriptorsMap.Add(InContainerCollectionInstanceDescriptor.ID, InContainerCollectionInstanceDescriptor);

				if (!ContainerCollectionDescriptor.ActorDescCollection)
				{
					ContainerCollectionDescriptor.ActorDescCollection = ContainerCollectionInstanceDescriptor.ActorDescCollection;

					// Gather actor descriptor views for this container
					CreateActorDescViewMap(*ContainerCollectionInstanceDescriptor.ActorDescCollection, ContainerCollectionDescriptor.ActorDescViewMap, ContainerCollectionDescriptor.EditorOnlyActorDescMap, ContainerCollectionInstanceDescriptor.ID, ContainerCollectionDescriptor.ContainerCollectionInstanceViews);

					// Resolve actor descriptor views before validation
					ResolveContainerDescriptor(ContainerCollectionDescriptor);

					// Validate container, fixing anything illegal, etc.
					ValidateContainerDescriptor(ContainerCollectionDescriptor, ContainerCollectionInstanceDescriptor.ID.IsMainContainer());

					// Update container, computing cluster, bounds, etc.
					UpdateContainerDescriptor(ContainerCollectionDescriptor);
				}

				// Calculate Bounds of Non-container ActorDescViews
				check(!ContainerCollectionInstanceDescriptor.Bounds.IsValid);
				ContainerCollectionDescriptor.ActorDescViewMap.ForEachActorDescView([&ContainerCollectionInstanceDescriptor](const FWorldPartitionActorDescView& ActorDescView)
				{
					if (ActorDescView.GetIsSpatiallyLoaded())
					{
						const FBox RuntimeBounds = ActorDescView.GetRuntimeBounds();
						check(RuntimeBounds.IsValid);

						ContainerCollectionInstanceDescriptor.Bounds += RuntimeBounds.TransformBy(ContainerCollectionInstanceDescriptor.Transform);
					}
				});
			}

			// Parse actor containers
			TArray<FWorldPartitionActorDescView> ContainerCollectionInstanceViews = ContainerCollectionDescriptor.ContainerCollectionInstanceViews;
			for (const FWorldPartitionActorDescView& ContainerCollectionInstanceView : ContainerCollectionInstanceViews)
			{
				FWorldPartitionActorDesc::FContainerInstance SubContainerInstance;
				if (!ContainerCollectionInstanceView.GetContainerInstance(SubContainerInstance) || !SubContainerInstance.Container)
				{
					ErrorHandler->OnLevelInstanceInvalidWorldAsset(ContainerCollectionInstanceView, ContainerCollectionInstanceView.GetContainerPackage(), IStreamingGenerationErrorHandler::ELevelInstanceInvalidReason::WorldAssetHasInvalidContainer);
					continue;
				}

				bool bContainerWasAlreadyInSet;
				ContainerInstancesStack.Add(SubContainerInstance.Container->ContainerPackageName, &bContainerWasAlreadyInSet);

				if (bContainerWasAlreadyInSet)
				{
					ErrorHandler->OnLevelInstanceInvalidWorldAsset(ContainerCollectionInstanceView, ContainerCollectionInstanceView.GetContainerPackage(), IStreamingGenerationErrorHandler::ELevelInstanceInvalidReason::CirculalReference);
					continue;
				}

				FContainerCollectionInstanceDescriptor SubContainerInstanceDescriptor;
				SubContainerInstanceDescriptor.ID = FActorContainerID(InContainerCollectionInstanceDescriptor.ID, ContainerCollectionInstanceView.GetGuid());
				SubContainerInstanceDescriptor.ActorDescCollection = MakeShared<FStreamingGenerationActorDescCollection>(FStreamingGenerationActorDescCollection{SubContainerInstance.Container});
				SubContainerInstanceDescriptor.Transform = SubContainerInstance.Transform * InContainerCollectionInstanceDescriptor.Transform;
				SubContainerInstanceDescriptor.ParentID = InContainerCollectionInstanceDescriptor.ID;
				SubContainerInstanceDescriptor.OwnerName = *ContainerCollectionInstanceView.GetActorLabelOrName().ToString();
				// Since Content Bundles streaming generation happens in its own context, all actor set instances must have the same content bundle GUID for now, so Level Instances
				// placed inside a Content Bundle will propagate their Content Bundle GUID to child instances.
				SubContainerInstanceDescriptor.ContentBundleID = InContainerCollectionInstanceDescriptor.ContentBundleID;
				SubContainerInstanceDescriptor.InstanceData = InheritParentContainerPerInstanceData(InContainerCollectionInstanceDescriptor.InstanceData, ContainerCollectionInstanceView);

				if (WorldPartitionSubsystem && InContainerCollectionInstanceDescriptor.ID.IsMainContainer() && ContainerCollectionInstanceView.GetContainerFilterType() == EWorldPartitionActorFilterType::Loading)
				{
					if (const FWorldPartitionActorFilter* ContainerFilter = ContainerCollectionInstanceView.GetContainerFilter())
					{						
						ContainerFilteredActors.Append(WorldPartitionSubsystem->GetFilteredActorsPerContainer(SubContainerInstanceDescriptor.ID, ContainerCollectionInstanceView.GetContainerPackage().ToString(), *ContainerFilter));
					}
				}

				CreateActorDescriptorViewsRecursive(SubContainerInstanceDescriptor);

				verify(ContainerInstancesStack.Remove(SubContainerInstance.Container->ContainerPackageName));
			}
		}

		// Fetch the versions stored in the map as it can have been reallocated during recursion
		FContainerCollectionDescriptor& ContainerCollectionDescriptor = ContainerCollectionDescriptorsMap.FindOrAdd(InContainerCollectionInstanceDescriptor.ActorDescCollection->GetMainContainerPackageName());
		FContainerCollectionInstanceDescriptor& ContainerCollectionInstanceDescriptor = ContainerCollectionInstanceDescriptorsMap.FindChecked(InContainerCollectionInstanceDescriptor.ID);

		if (!InContainerCollectionInstanceDescriptor.ID.IsMainContainer())
		{
			FContainerCollectionInstanceDescriptor& ParentContainerCollection = ContainerCollectionInstanceDescriptorsMap.FindChecked(InContainerCollectionInstanceDescriptor.ParentID);
			ParentContainerCollection.Bounds += ContainerCollectionInstanceDescriptor.Bounds;
		}

		// Apply per-instance data
		ContainerCollectionInstanceDescriptor.PerInstanceData.Reserve(ContainerCollectionDescriptor.ActorDescViewMap.Num());
		ContainerCollectionDescriptor.ActorDescViewMap.ForEachActorDescView([&ContainerCollectionInstanceDescriptor, &InheritParentContainerPerInstanceData](FWorldPartitionActorDescView& ActorDescView)
		{
			const FContainerCollectionInstanceDescriptor::FPerInstanceData PerInstanceData = InheritParentContainerPerInstanceData(ContainerCollectionInstanceDescriptor.InstanceData, ActorDescView);

			if (PerInstanceData != ContainerCollectionInstanceDescriptor.InstanceData)
			{
				const FSetElementId UniquePerInstanceDataId = ContainerCollectionInstanceDescriptor.UniquePerInstanceData.Add(PerInstanceData);
				ContainerCollectionInstanceDescriptor.PerInstanceData.Emplace(ActorDescView.GetGuid(), UniquePerInstanceDataId);
			}
		});

		// Validate container instance, fixing anything illegal, etc.
		ValidateContainerInstanceDescriptor(ContainerCollectionInstanceDescriptor, ContainerCollectionInstanceDescriptor.ID.IsMainContainer());
	}

	/** 
	 * Creates the actor descriptor views for the specified container.
	 */
	void CreateActorContainers(const FStreamingGenerationActorDescCollection& InActorDescCollection)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWorldPartitionStreamingGenerator::CreateActorContainers);

		FContainerCollectionInstanceDescriptor MainContainerCollectionInstance;
		MainContainerCollectionInstance.ActorDescCollection = MakeShared<FStreamingGenerationActorDescCollection>(InActorDescCollection);
		MainContainerCollectionInstance.ClusterMode = EContainerClusterMode::Partitioned;
		MainContainerCollectionInstance.OwnerName = TEXT("MainContainer");
		MainContainerCollectionInstance.ContentBundleID = InActorDescCollection.GetContentBundleGuid();
		MainContainerCollectionInstance.InstanceData.bIsSpatiallyLoaded = true; // Since we apply AND logic on spatially loaded flag recursively, startup value must be true

		CreateActorDescriptorViewsRecursive(MainContainerCollectionInstance);
	}

	/** 
	 * Perform various validations on the container descriptor, and adjust it based on different requirements. This needs to happen before updating
	 * containers bounds because some actor descriptor views might change grid placement, etc.
	 */
	void ResolveContainerDescriptor(FContainerCollectionDescriptor& ContainerCollectionDescriptor)
	{
		auto ResolveActorDescView = [this, &ContainerCollectionDescriptor](FWorldPartitionActorDescView& ActorDescView)
		{
			ResolveRuntimeSpatiallyLoaded(ActorDescView);
			ResolveRuntimeGrid(ActorDescView);
			ResolveRuntimeDataLayers(ActorDescView, ContainerCollectionDescriptor.ActorDescViewMap);
		};

		ContainerCollectionDescriptor.ActorDescViewMap.ForEachActorDescView([this, &ResolveActorDescView](FWorldPartitionActorDescView& ActorDescView)
		{
			ResolveActorDescView(ActorDescView);
		});

		for (FWorldPartitionActorDescView& ContainerCollectionInstanceView : ContainerCollectionDescriptor.ContainerCollectionInstanceViews)
		{
			ResolveActorDescView(ContainerCollectionInstanceView);
		}
	}

	/** 
	 * Perform various validations on the container descriptor, and adjust it based on different requirements. This needs to happen before updating
	 * containers bounds because some actor descriptor views might change grid placement, etc.
	 */
	void ValidateContainerDescriptor(FContainerCollectionDescriptor& ContainerCollectionDescriptor, bool bIsMainContainer)
	{
		if (bIsMainContainer)
		{
			TArray<FGuid> LevelScriptReferences;
			if (WorldPartitionContext)
			{
				// Gather all references to external actors from the level script and make them always loaded
				if (ULevelScriptBlueprint* LevelScriptBlueprint = WorldPartitionContext->GetTypedOuter<UWorld>()->PersistentLevel->GetLevelScriptBlueprint(true))
				{
					const ActorsReferencesUtils::FGetActorReferencesParams Params = ActorsReferencesUtils::FGetActorReferencesParams(LevelScriptBlueprint)
						.SetRequiredFlags(RF_HasExternalPackage);
					TArray<ActorsReferencesUtils::FActorReference> LevelScriptExternalActorReferences = ActorsReferencesUtils::GetActorReferences(Params);
					Algo::Transform(LevelScriptExternalActorReferences, LevelScriptReferences, [](const ActorsReferencesUtils::FActorReference& ActorReference) { return ActorReference.Actor->GetActorGuid(); });
				}

				// Validate data layers
				if (DataLayerManager)
				{
					DataLayerManager->ForEachDataLayerInstance([this](const UDataLayerInstance* DataLayerInstance)
					{
						DataLayerInstance->Validate(ErrorHandler);
						return true;
					});
				}
			}
			else
			{
				ULevel::GetLevelScriptExternalActorsReferencesFromPackage(ContainerCollectionDescriptor.ActorDescCollection->GetMainContainerPackageName(), LevelScriptReferences);
			}

			for (const FGuid& LevelScriptReferenceActorGuid : LevelScriptReferences)
			{
				if (FWorldPartitionActorDescView* ActorDescView = ContainerCollectionDescriptor.ActorDescViewMap.FindByGuid(LevelScriptReferenceActorGuid))
				{
					if (ActorDescView->GetIsSpatiallyLoaded())
					{
						ErrorHandler->OnInvalidReferenceLevelScriptStreamed(*ActorDescView);
						ActorDescView->SetForcedNonSpatiallyLoaded();
					}

					if (ActorDescView->GetRuntimeDataLayerInstanceNames().Num())
					{
						ErrorHandler->OnInvalidReferenceLevelScriptDataLayers(*ActorDescView);
						ActorDescView->SetInvalidDataLayers();
					}
				}
			}
		}

		// Route standard CheckForErrors calls which should not modify actor descriptors in any ways
		ContainerCollectionDescriptor.ActorDescViewMap.ForEachActorDescView([this](FWorldPartitionActorDescView& ActorDescView)
		{
			ActorDescView.CheckForErrors(ErrorHandler);
		});

		for (FWorldPartitionActorDescView& ContainerCollectionInstanceView : ContainerCollectionDescriptor.ContainerCollectionInstanceViews)
		{
			ContainerCollectionInstanceView.CheckForErrors(ErrorHandler);
		}

		// Perform various adjustements based on validations and report errors
		//
		// The first validation pass is used to report errors, subsequent passes are used to make corrections to the actor descriptor views.
		// Since the references can form cycles/long chains in the data fixes might need to be propagated in multiple passes.
		// 
		// This works because fixes are deterministic and always apply the same way to both Actors being modified, so there's no ordering issues possible.
		int32 NbErrorsDetected = INDEX_NONE;
		for(uint32 NbValidationPasses = 0; NbErrorsDetected; NbValidationPasses++)
		{
			// Type of work performed in this pass, for clarity
			enum class EPassType
			{
				ErrorReporting,
				Fixup
			};
			const EPassType PassType = NbValidationPasses == 0 ? EPassType::ErrorReporting : EPassType::Fixup;

			NbErrorsDetected = 0;

			ContainerCollectionDescriptor.ActorDescViewMap.ForEachActorDescView([this, &ContainerCollectionDescriptor, &NbErrorsDetected, PassType](FWorldPartitionActorDescView& ActorDescView)
			{
				// Validate grid placement
				auto IsReferenceGridPlacementValid = [](const FWorldPartitionActorDescView& RefererActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
				{
					const bool bIsActorDescSpatiallyLoaded = RefererActorDescView.GetIsSpatiallyLoaded();
					const bool bIsActorDescRefSpatiallyLoaded = ReferenceActorDescView.GetIsSpatiallyLoaded();
					return bIsActorDescSpatiallyLoaded == bIsActorDescRefSpatiallyLoaded;
				};

				// Validate data layers
				auto IsReferenceDataLayersValid = [](const FWorldPartitionActorDescView& RefererActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
				{
					if (RefererActorDescView.GetRuntimeDataLayerInstanceNames().Num() == ReferenceActorDescView.GetRuntimeDataLayerInstanceNames().Num())
					{
						const TSet<FName> RefererActorDescDataLayers(RefererActorDescView.GetRuntimeDataLayerInstanceNames());
						const TSet<FName> ReferenceActorDescDataLayers(ReferenceActorDescView.GetRuntimeDataLayerInstanceNames());

						return RefererActorDescDataLayers.Includes(ReferenceActorDescDataLayers);
					}

					return false;
				};

				// Validate runtime grid references
				auto IsReferenceRuntimeGridValid = [](const FWorldPartitionActorDescView& RefererActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
				{
					return RefererActorDescView.GetRuntimeGrid() == ReferenceActorDescView.GetRuntimeGrid();
				};

				// Build references List
				struct FActorReferenceInfo
				{
					FGuid ActorGuid;
					FWorldPartitionActorDescView* ActorDesc;
					FGuid ReferenceGuid;
					FWorldPartitionActorDescView* ReferenceActorDesc;
				};

				TArray<FActorReferenceInfo> References;

				// Add normal actor references
				for (const FGuid& ReferenceGuid : ActorDescView.GetReferences())
				{
					if (ReferenceGuid != ActorDescView.GetParentActor()) // References to the parent are inversed in their handling 
					{
						// Filter out parent back references
						FWorldPartitionActorDescView* ReferenceActorDesc = ContainerCollectionDescriptor.ActorDescViewMap.FindByGuid(ReferenceGuid);
						if (!ReferenceActorDesc || (ReferenceActorDesc->GetParentActor() != ActorDescView.GetGuid()))
						{
							References.Emplace(FActorReferenceInfo { ActorDescView.GetGuid(), &ActorDescView, ReferenceGuid, ReferenceActorDesc });
						}
					}
				}

				// Add attach reference for the topmost parent, this reference is inverted since we consider the top most existing 
				// parent to be refering to us, not the child to be referering the parent.
				{
					FGuid ParentGuid = ActorDescView.GetParentActor();
					FWorldPartitionActorDescView* TopParentDescView = nullptr;

					while (ParentGuid.IsValid())
					{
						FWorldPartitionActorDescView* ParentDescView = ContainerCollectionDescriptor.ActorDescViewMap.FindByGuid(ParentGuid);
					
						if (ParentDescView)
						{
							TopParentDescView = ParentDescView;
							ParentGuid = ParentDescView->GetParentActor();
						}
						else
						{
							if (PassType == EPassType::ErrorReporting)
							{
								// We had a guid but parent cannot be found, this will report a missing reference error, but no error in the subsequent passes
								References.Emplace(FActorReferenceInfo{ ActorDescView.GetGuid(), &ActorDescView, ParentGuid, nullptr });
							}

							break; 
						}
					}

					if (TopParentDescView)
					{
						References.Emplace(FActorReferenceInfo { TopParentDescView->GetGuid(), TopParentDescView, ActorDescView.GetGuid(), &ActorDescView });
					}
				}

				TArray<FGuid> RuntimeReferences;
				TArray<FGuid> EditorReferences;

				if (PassType == EPassType::Fixup)
				{
					RuntimeReferences.Reserve(ActorDescView.GetReferences().Num());
					EditorReferences.Reserve(ActorDescView.GetReferences().Num());
				}

				for (FActorReferenceInfo& Info : References)
				{
					FWorldPartitionActorDescView* RefererActorDescView = Info.ActorDesc;
					FWorldPartitionActorDescView* ReferenceActorDescView = Info.ReferenceActorDesc;

					if (ReferenceActorDescView)
					{
						// The actor reference is not editor-only, but we are referencing it through an editor-only property
						if (RefererActorDescView->IsEditorOnlyReference(ReferenceActorDescView->GetGuid()))
						{
							if (PassType == EPassType::Fixup)
							{
								EditorReferences.Add(Info.ReferenceGuid);
							}

							NbErrorsDetected++;
						}
						else
						{
							// Validate grid placement
							if (!IsReferenceGridPlacementValid(*RefererActorDescView, *ReferenceActorDescView))
							{
								if (PassType == EPassType::ErrorReporting)
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
								if (PassType == EPassType::ErrorReporting)
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
								if (PassType == EPassType::ErrorReporting)
								{
									ErrorHandler->OnInvalidReferenceRuntimeGrid(*RefererActorDescView, *ReferenceActorDescView);
								}
								else
								{
									RefererActorDescView->SetForcedNoRuntimeGrid();
									ReferenceActorDescView->SetForcedNoRuntimeGrid();
								}

								NbErrorsDetected++;
							}

							if (PassType == EPassType::Fixup)
							{
								RuntimeReferences.Add(Info.ReferenceGuid);
							}
						}
					}
					else
					{
						if (!ContainerCollectionDescriptor.EditorOnlyActorDescMap.Contains(Info.ReferenceGuid))
						{
							if (PassType == EPassType::ErrorReporting)
							{
								FWorldPartitionActorDescView ReferendceActorDescView;
								FWorldPartitionActorDescView* ReferendceActorDescViewPtr = nullptr;

								if (const UActorDescContainer** ExistingReferenceContainerPtr = ActorGuidsToContainerMap.Find((Info.ReferenceGuid)))
								{ 
									if (const FWorldPartitionActorDesc* ReferenceActorDesc = (*ExistingReferenceContainerPtr)->GetActorDesc(Info.ReferenceGuid))
									{
										ReferendceActorDescView = FWorldPartitionActorDescView(ReferenceActorDesc);
										ReferendceActorDescViewPtr = &ReferendceActorDescView;
									}
								}

								ErrorHandler->OnInvalidReference(*RefererActorDescView, Info.ReferenceGuid, ReferendceActorDescViewPtr);
							}
						}
						else if (PassType == EPassType::Fixup)
						{
							EditorReferences.Add(Info.ReferenceGuid);
						}

						NbErrorsDetected++;
					}
				}

				if (PassType == EPassType::Fixup)
				{
					if (RuntimeReferences.Num() != ActorDescView.GetReferences().Num())
					{
						ActorDescView.SetRuntimeReferences(RuntimeReferences);
						ActorDescView.SetEditorReferences(EditorReferences);
					}					
				}
			});		
		}
	}

	/** 
	 * Perform various validations on the container descriptor instance, and adjust it based on different requirements. This needs to happen before updating
	 * containers bounds because some actor descriptor views might change grid placement, etc.
	 */
	void ValidateContainerInstanceDescriptor(FContainerCollectionInstanceDescriptor& ContainerCollectionInstanceDescriptor, bool bIsMainContainer)
	{
		FContainerCollectionDescriptor& ContainerCollectionDescriptor = ContainerCollectionDescriptorsMap.FindChecked(ContainerCollectionInstanceDescriptor.ActorDescCollection->GetMainContainerPackageName());

		// Perform various adjustements based on validations and report errors
		//
		// The first validation pass is used to report errors, subsequent passes are used to make corrections to the actor descriptor views.
		// Since the references can form cycles/long chains in the data fixes might need to be propagated in multiple passes.
		// 
		// This works because fixes are deterministic and always apply the same way to both Actors being modified, so there's no ordering issues possible.
		int32 NbErrorsDetected = INDEX_NONE;
		for(uint32 NbValidationPasses = 0; NbErrorsDetected; NbValidationPasses++)
		{
			// Type of work performed in this pass, for clarity
			enum class EPassType
			{
				ErrorReporting,
				Fixup
			};
			const EPassType PassType = NbValidationPasses == 0 ? EPassType::ErrorReporting : EPassType::Fixup;

			NbErrorsDetected = 0;

			ContainerCollectionDescriptor.ActorDescViewMap.ForEachActorDescView([this, &ContainerCollectionDescriptor, &ContainerCollectionInstanceDescriptor, &NbErrorsDetected, PassType](FWorldPartitionActorDescView& ActorDescView)
			{
				FContainerCollectionInstanceDescriptor::FPerInstanceData& PerInstanceData = ContainerCollectionInstanceDescriptor.GetInstanceData(ActorDescView.GetGuid());

PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (ActorDescView.ShouldValidateRuntimeGrid())
PRAGMA_ENABLE_DEPRECATION_WARNINGS
				{
					if (!IsValidGrid(PerInstanceData.RuntimeGrid))
					{
						if (PassType == EPassType::ErrorReporting)
						{
							ErrorHandler->OnInvalidRuntimeGrid(ActorDescView, ActorDescView.GetRuntimeGrid());
						}
						else
						{
							PerInstanceData.RuntimeGrid = NAME_None;
						}

						NbErrorsDetected++;
					}
				}

				if (TSet<FGuid>* FilteredActors = ContainerFilteredActors.Find(ContainerCollectionInstanceDescriptor.ID))
				{
					const bool IsReferencerFiltered = FilteredActors->Contains(ActorDescView.GetGuid());
					for (const FGuid& ReferenceGuid : ActorDescView.GetReferences())
					{
						const bool IsReferenceeFiltered = FilteredActors->Contains(ReferenceGuid);
						if (IsReferenceeFiltered && !IsReferencerFiltered)
						{
							if (PassType == EPassType::ErrorReporting)
							{
								const FWorldPartitionActorDescView& ReferenceActorDesc = ContainerCollectionDescriptor.ActorDescViewMap.FindByGuidChecked(ReferenceGuid);
								ErrorHandler->OnInvalidActorFilterReference(ActorDescView, ReferenceActorDesc);
							}
							else
							{
								FilteredActors->Remove(ReferenceGuid);
							}

							NbErrorsDetected++;
						}
					}
				}
			});
		}
	}

	/** 
	 * Update the container descriptor containers to adjust their bounds from actor descriptor views.
	 */
	void UpdateContainerDescriptor(FContainerCollectionDescriptor& ContainerCollectionDescriptor)
	{
		// Build clusters for this container - at this point, all actors references should be in the same data layers, grid, etc because of actor descriptors validation.
		TArray<TPair<FGuid, TArray<FGuid>>> ActorsWithRefs;
		ContainerCollectionDescriptor.ActorDescViewMap.ForEachActorDescView([&ActorsWithRefs](const FWorldPartitionActorDescView& ActorDescView) { ActorsWithRefs.Emplace(ActorDescView.GetGuid(), ActorDescView.GetReferences()); });
		ContainerCollectionDescriptor.Clusters = GenerateObjectsClusters(ActorsWithRefs);
	}

public:
	struct FWorldPartitionStreamingGeneratorParams
	{
		FWorldPartitionStreamingGeneratorParams()
			: WorldPartitionContext(nullptr)
			, ModifiedActorsDescList(nullptr)
			, ErrorHandler(&NullErrorHandler)
			, bEnableStreaming(false)
		{}

		const UWorldPartition* WorldPartitionContext;
		FActorDescList* ModifiedActorsDescList;
		IStreamingGenerationErrorHandler* ErrorHandler;
		bool bEnableStreaming;
		TMap<FGuid, const UActorDescContainer*> ActorGuidsToContainerMap;
		TArray<TSubclassOf<AActor>> FilteredClasses;
		TFunction<bool(FName)> IsValidGrid;

		FWorldPartitionStreamingGeneratorParams& SetWorldPartitionContext(const UWorldPartition* InWorldPartitionContext) { WorldPartitionContext = InWorldPartitionContext; return *this; }
		FWorldPartitionStreamingGeneratorParams& SetModifiedActorsDescList(FActorDescList* InModifiedActorsDescList) { ModifiedActorsDescList = InModifiedActorsDescList; return *this; }
		FWorldPartitionStreamingGeneratorParams& SetErrorHandler(IStreamingGenerationErrorHandler* InErrorHandler) { ErrorHandler = InErrorHandler; return *this; }
		FWorldPartitionStreamingGeneratorParams& SetEnableStreaming(bool bInEnableStreaming) { bEnableStreaming = bInEnableStreaming; return *this; }
		FWorldPartitionStreamingGeneratorParams& SetActorGuidsToContainerMap(const TMap<FGuid, const UActorDescContainer*>& InActorGuidsToContainerMap) { ActorGuidsToContainerMap = InActorGuidsToContainerMap; return *this; }
		FWorldPartitionStreamingGeneratorParams& SetFilteredClasses(const TArray<TSubclassOf<AActor>>& InFilteredClasses) { FilteredClasses = InFilteredClasses; return *this; }
		FWorldPartitionStreamingGeneratorParams& SetIsValidGrid(TFunction<bool(FName)> InIsValidGrid) { IsValidGrid = InIsValidGrid; return *this; }

		inline static FStreamingGenerationNullErrorHandler NullErrorHandler;
	};

	FWorldPartitionStreamingGenerator(const FWorldPartitionStreamingGeneratorParams& Params)
		: WorldPartitionContext(Params.WorldPartitionContext)
		, bEnableStreaming(Params.bEnableStreaming)
		, ModifiedActorsDescList(Params.ModifiedActorsDescList)
		, FilteredClasses(Params.FilteredClasses)
		, IsValidGrid(Params.IsValidGrid)
		, ErrorHandler(Params.ErrorHandler)
		, ActorGuidsToContainerMap(Params.ActorGuidsToContainerMap)
	{
		UWorld* OwningWorld = WorldPartitionContext ? WorldPartitionContext->GetWorld() : nullptr;
		WorldPartitionSubsystem = OwningWorld ? UWorld::GetSubsystem<UWorldPartitionSubsystem>(OwningWorld) : nullptr;
		DataLayerManager = OwningWorld ? WorldPartitionContext->GetDataLayerManager() : nullptr;
	}

	void PreparationPhase(const FStreamingGenerationActorDescCollection& ActorDescCollection)
	{
		CreateActorContainers(ActorDescCollection);
	}

	static TUniquePtr<FArchive> CreateDumpStateLogArchive(const TCHAR* Suffix, bool bTimeStamped = true)
	{
		FString StateLogOutputFilename = FPaths::ProjectLogDir() / TEXT("WorldPartition") / FString::Printf(TEXT("StreamingGeneration-%s"), Suffix);

		if (bTimeStamped)
		{			
			StateLogOutputFilename += FString::Printf(TEXT("-%08x-%s"), FPlatformProcess::GetCurrentProcessId(), *FDateTime::Now().ToIso8601().Replace(TEXT(":"), TEXT(".")));
		}

		StateLogOutputFilename += TEXT(".log");
		return TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*StateLogOutputFilename));
	}

	void DumpStateLog(FHierarchicalLogArchive& Ar)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWorldPartitionStreamingGenerator::DumpStateLog);

		// Build the containers tree representation
		TMultiMap<FActorContainerID, FActorContainerID> InvertedContainersHierarchy;
		for (auto& [ContainerID, ContainerCollectionInstanceDescriptor] : ContainerCollectionInstanceDescriptorsMap)
		{
			if (!ContainerID.IsMainContainer())
			{
				InvertedContainersHierarchy.Add(ContainerCollectionInstanceDescriptor.ParentID, ContainerID);
			}
		}

		UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("Containers:")));
		for (auto& [LevelName, ContainerCollectionDescriptor] : ContainerCollectionDescriptorsMap)
		{
			UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("Container: %s"), *ContainerCollectionDescriptor.ActorDescCollection->GetMainContainerPackageName().ToString()));

			if (ContainerCollectionDescriptor.ActorDescViewMap.Num())
			{
				UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("ActorDescs:")));

				TMap<FGuid, FWorldPartitionActorDescView*> SortedActorDescViewMap = ContainerCollectionDescriptor.ActorDescViewMap.ActorDescViewsByGuid;
				SortedActorDescViewMap.KeySort([](const FGuid& GuidA, const FGuid& GuidB) { return GuidA < GuidB; });

				for (auto& [ActorGuid, ActorDescView] : SortedActorDescViewMap)
				{
					Ar.Print(*ActorDescView->ToString());
				}
			}

			if (ContainerCollectionDescriptor.Clusters.Num())
			{
				UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("Clusters:")));

				TArray<TArray<FGuid>> SortedClusters = ContainerCollectionDescriptor.Clusters;
				for (TArray<FGuid>& ActorGuids : SortedClusters)
				{
					ActorGuids.Sort([](const FGuid& GuidA, const FGuid& GuidB) { return GuidA < GuidB; });
				}
				SortedClusters.Sort([](const TArray<FGuid>& GuidA, const TArray<FGuid>& GuidB) { return GuidA[0] < GuidB[0]; });

				int ClusterIndex = 0;
				for (TArray<FGuid>& ActorGuids : SortedClusters)
				{
					UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("[%3d]"), ClusterIndex++));
					for (const FGuid& ActorGuid : ActorGuids)
					{
						const FWorldPartitionActorDescView& ActorDescView = ContainerCollectionDescriptor.ActorDescViewMap.FindByGuidChecked(ActorGuid);
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
				const FContainerCollectionInstanceDescriptor& ContainerInstanceDescriptor = ContainerCollectionInstanceDescriptorsMap.FindChecked(ContainerID);
				
				{
					UE_SCOPED_INDENT_LOG_ARCHIVE(Ar.PrintfIndent(TEXT("%s:"), *ContainerInstanceDescriptor.OwnerName));

					Ar.Printf(TEXT("       ID: %s"), *ContainerInstanceDescriptor.ID.ToString());
					Ar.Printf(TEXT(" ParentID: %s"), *ContainerInstanceDescriptor.ParentID.ToString());
					Ar.Printf(TEXT("   Bounds: %s"), *ContainerInstanceDescriptor.Bounds.ToString());
					Ar.Printf(TEXT("Transform: %s"), *ContainerInstanceDescriptor.Transform.ToString());
					Ar.Printf(TEXT("Container: %s"), *ContainerInstanceDescriptor.ActorDescCollection->GetMainContainerPackageName().ToString());
				}

				TArray<FActorContainerID> ChildContainersIDs;
				InvertedContainersHierarchy.MultiFind(ContainerID, ChildContainersIDs);
				ChildContainersIDs.Sort();

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

	const FStreamingGenerationContext* GetStreamingGenerationContext(const FStreamingGenerationActorDescCollection& ActorDescCollection)
	{
		if (!StreamingGenerationContext)
		{
			// Construct the streaming generation context
			StreamingGenerationContext = MakeUnique<FStreamingGenerationContext>(this, ActorDescCollection);
		}

		return StreamingGenerationContext.Get();
	}

	TArray<const UDataLayerInstance*> GetRuntimeDataLayerInstances(const TArray<FName>& RuntimeDataLayers) const
	{
		return DataLayerManager ? DataLayerManager->GetRuntimeDataLayerInstances(RuntimeDataLayers) : TArray<const UDataLayerInstance*>();
	}

private:
	const UWorldPartition* WorldPartitionContext;
	UWorldPartitionSubsystem* WorldPartitionSubsystem;
	const UDataLayerManager* DataLayerManager;
	bool bEnableStreaming;
	FActorDescList* ModifiedActorsDescList;
	TArray<TSubclassOf<AActor>> FilteredClasses;
	TFunction<bool(FName)> IsValidGrid;
	IStreamingGenerationErrorHandler* ErrorHandler;

	// Maps a level to its descriptor
	TMap<FName, FContainerCollectionDescriptor> ContainerCollectionDescriptorsMap;
	
	/** Maps containers IDs to their container collection instance descriptor */
	TMap<FActorContainerID, FContainerCollectionInstanceDescriptor> ContainerCollectionInstanceDescriptorsMap;

	/** Data required for streaming generation interface */
	TUniquePtr<FStreamingGenerationContext> StreamingGenerationContext;

	/** List of containers participating in this streaming generation step */
	TMap<FGuid, const UActorDescContainer*> ActorGuidsToContainerMap;

	/** List of current container instances on the stack to detect circular references */
	TSet<FName> ContainerInstancesStack;

	/** Maps containers IDs to their filtered actors use while creating FContainerCollectionInstanceDescriptor */
	TMap<FActorContainerID, TSet<FGuid>> ContainerFilteredActors;
};

bool UWorldPartition::GenerateStreaming(TArray<FString>* OutPackagesToGenerate)
{
	FGenerateStreamingParams Params = FGenerateStreamingParams().SetActorDescContainer(nullptr);
	FGenerateStreamingContext Context = FGenerateStreamingContext().SetPackagesToGenerate(OutPackagesToGenerate);
	return GenerateStreaming(Params, Context);
}

bool UWorldPartition::GenerateContainerStreaming(const UActorDescContainer* InActorDescContainer, TArray<FString>* OutPackagesToGenerate)
{
	FGenerateStreamingParams Params = FGenerateStreamingParams().SetActorDescContainer(InActorDescContainer);
	FGenerateStreamingContext Context = FGenerateStreamingContext().SetPackagesToGenerate(OutPackagesToGenerate);
	return GenerateContainerStreaming(Params, Context);
}

bool UWorldPartition::GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingContext& InContext)
{
	check(InParams.ActorDescCollection.IsEmpty());
	FGenerateStreamingParams Params = FGenerateStreamingParams(InParams).SetActorDescContainer(ActorDescContainer);

	OnPreGenerateStreaming.Broadcast(InContext.PackagesToGenerate);

	return GenerateContainerStreaming(Params, InContext);
}

bool UWorldPartition::GenerateContainerStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingContext& InContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartition::GenerateContainerStreaming);

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

	const FString ContainerPackageName = InParams.ActorDescCollection.GetMainContainerPackageName().ToString();
	FString ContainerShortName = FPackageName::GetShortName(ContainerPackageName);
	if (!ContainerPackageName.StartsWith(TEXT("/Game/")))
	{
		TArray<FString> SplitContainerPath;
		if (ContainerPackageName.ParseIntoArray(SplitContainerPath, TEXT("/")))
		{
			ContainerShortName += TEXT(".");
			ContainerShortName += SplitContainerPath[0];
		}
	}

	UE_SCOPED_TIMER(*FString::Printf(TEXT("GenerateStreaming for '%s'"), *ContainerShortName), LogWorldPartition, Display);

	// Dump state log
	TUniquePtr<FArchive> LogFileAr;
	TUniquePtr<FHierarchicalLogArchive> HierarchicalLogAr;

	if (IsMainWorldPartition() && (!GIsBuildMachine || IsRunningCookCommandlet()))
	{
		TStringBuilder<256> StateLogSuffix;
		StateLogSuffix += bIsPIE ? TEXT("PIE") : (IsRunningGame() ? TEXT("Game") : (IsRunningCookCommandlet() ? TEXT("Cook") : (GIsAutomationTesting ? TEXT("UnitTest") : TEXT("Manual"))));
		StateLogSuffix += TEXT("_");
		StateLogSuffix += ContainerShortName;
		LogFileAr = FWorldPartitionStreamingGenerator::CreateDumpStateLogArchive(*StateLogSuffix, !InParams.OutputLogPath);
		
		InContext.OutputLogFilename = LogFileAr->GetArchiveName();
		HierarchicalLogAr = MakeUnique<FHierarchicalLogArchive>(*LogFileAr);
	}

	FWorldPartitionStreamingGenerator::FWorldPartitionStreamingGeneratorParams StreamingGeneratorParams = FWorldPartitionStreamingGenerator::FWorldPartitionStreamingGeneratorParams()
		.SetWorldPartitionContext(this)
		.SetModifiedActorsDescList( ModifiedActorsDescList)
		.SetIsValidGrid([this](FName GridName) { return RuntimeHash->IsValidGrid(GridName); })
		.SetErrorHandler(StreamingGenerationErrorHandlerOverride ? (*StreamingGenerationErrorHandlerOverride)(ErrorHandler) : ErrorHandler)
		.SetEnableStreaming(IsStreamingEnabled());

	FWorldPartitionStreamingGenerator StreamingGenerator(StreamingGeneratorParams);

	// Preparation Phase
	StreamingGenerator.PreparationPhase(InParams.ActorDescCollection);

	if (HierarchicalLogAr.IsValid())
	{
		StreamingGenerator.DumpStateLog(*HierarchicalLogAr);
	}

	// Generate streaming
	check(!StreamingPolicy);
	StreamingPolicy = NewObject<UWorldPartitionStreamingPolicy>(const_cast<UWorldPartition*>(this), WorldPartitionStreamingPolicyClass.Get(), NAME_None, bIsPIE ? RF_Transient : RF_NoFlags);

	check(RuntimeHash);
	if (RuntimeHash->GenerateStreaming(StreamingPolicy, StreamingGenerator.GetStreamingGenerationContext(InParams.ActorDescCollection), InContext.PackagesToGenerate))
	{
		if (HierarchicalLogAr.IsValid())
		{
			RuntimeHash->DumpStateLog(*HierarchicalLogAr);
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

URuntimeHashExternalStreamingObjectBase* UWorldPartition::FlushStreamingToExternalStreamingObject(const FString& ExternalStreamingObjectName)
{
	URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject = RuntimeHash->StoreToExternalStreamingObject(this, *ExternalStreamingObjectName);
	check(ExternalStreamingObject);

	StreamingPolicy->StoreToExternalStreamingObject(*ExternalStreamingObject);

	FlushStreaming();
	return ExternalStreamingObject;
}

void UWorldPartition::GenerateHLOD(ISourceControlHelper* SourceControlHelper, bool bCreateActorsOnly)
{
	ForEachActorDescContainer([this, &SourceControlHelper, bCreateActorsOnly](UActorDescContainer* InActorDescContainer)
	{
		FStreamingGenerationLogErrorHandler LogErrorHandler;
		FWorldPartitionStreamingGenerator::FWorldPartitionStreamingGeneratorParams StreamingGeneratorParams = FWorldPartitionStreamingGenerator::FWorldPartitionStreamingGeneratorParams()
			.SetWorldPartitionContext(this)
			.SetErrorHandler(StreamingGenerationErrorHandlerOverride ? (*StreamingGenerationErrorHandlerOverride)(&LogErrorHandler) : &LogErrorHandler)
			.SetEnableStreaming(IsStreamingEnabled())
			.SetFilteredClasses({ AWorldPartitionHLOD::StaticClass() })
			.SetIsValidGrid([this](FName GridName) { return RuntimeHash->IsValidGrid(GridName); });

		FWorldPartitionStreamingGenerator StreamingGenerator(StreamingGeneratorParams);
		FStreamingGenerationActorDescCollection ActorDescCollection{InActorDescContainer};
		StreamingGenerator.PreparationPhase(ActorDescCollection);

		TUniquePtr<FArchive> LogFileAr = FWorldPartitionStreamingGenerator::CreateDumpStateLogArchive(TEXT("HLOD"));
		FHierarchicalLogArchive HierarchicalLogAr(*LogFileAr);
		StreamingGenerator.DumpStateLog(HierarchicalLogAr);

		RuntimeHash->GenerateHLOD(SourceControlHelper, StreamingGenerator.GetStreamingGenerationContext(ActorDescCollection), bCreateActorsOnly);
	});	
}

void UWorldPartition::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	FActorDescList ModifiedActorDescList;

	FWorldPartitionStreamingGenerator::FWorldPartitionStreamingGeneratorParams StreamingGeneratorParams = FWorldPartitionStreamingGenerator::FWorldPartitionStreamingGeneratorParams()
		.SetWorldPartitionContext(this)
		.SetModifiedActorsDescList(&ModifiedActorDescList)
		.SetErrorHandler(StreamingGenerationErrorHandlerOverride ? (*StreamingGenerationErrorHandlerOverride)(ErrorHandler) : ErrorHandler)
		.SetIsValidGrid([this](FName GridName) { return RuntimeHash->IsValidGrid(GridName); })
		.SetEnableStreaming(IsStreamingEnabled());

	ForEachActorDescContainer([&StreamingGeneratorParams](const UActorDescContainer* InActorDescContainer)
	{
		for (FActorDescList::TConstIterator<> ActorDescIt(InActorDescContainer); ActorDescIt; ++ActorDescIt)
		{
			check(!StreamingGeneratorParams.ActorGuidsToContainerMap.Contains(ActorDescIt->GetGuid()));
			StreamingGeneratorParams.ActorGuidsToContainerMap.Add(ActorDescIt->GetGuid(), InActorDescContainer);
		}
	});

	ForEachActorDescContainer([this, &StreamingGeneratorParams](const UActorDescContainer* InActorDescContainer)
	{
		check(StreamingGeneratorParams.WorldPartitionContext == InActorDescContainer->GetWorldPartition());
		FWorldPartitionStreamingGenerator StreamingGenerator(StreamingGeneratorParams);
		FStreamingGenerationActorDescCollection Collection{InActorDescContainer};
		StreamingGenerator.PreparationPhase(Collection);
	});
	
}

FStreamingGenerationActorDescCollection::FStreamingGenerationActorDescCollection(std::initializer_list<TObjectPtr<const UActorDescContainer>> ActorDescContainerArray)
	: TActorDescContainerCollection<TObjectPtr<const UActorDescContainer>>(ActorDescContainerArray)
{
	SortCollection();
}

UWorld* FStreamingGenerationActorDescCollection::GetWorld() const
{
	if (const UActorDescContainer* MainContainer = GetMainActorDescContainer())
	{
		if (UWorldPartition* WorldPartition = MainContainer->GetWorldPartition())
		{
			return WorldPartition->GetWorld();
		}
	}

	return nullptr;
}

FGuid FStreamingGenerationActorDescCollection::GetContentBundleGuid() const
{
	if (const UActorDescContainer* MainContainer = GetMainActorDescContainer())
	{
		return MainContainer->GetContentBundleGuid();
	}

	return FGuid();
}

const UActorDescContainer* FStreamingGenerationActorDescCollection::GetMainActorDescContainer() const
{
	if (!IsEmpty())
	{
		return ActorDescContainerCollection[MainContainerIdx];
	}

	return nullptr;
}

FName FStreamingGenerationActorDescCollection::GetMainContainerPackageName() const
{
	if (const UActorDescContainer* MainContainer = GetMainActorDescContainer())
	{
		return MainContainer->GetContainerPackage();
	}

	return FName();
}

TArrayView<const UActorDescContainer*> FStreamingGenerationActorDescCollection::GetExternalDataLayerContainers()
{
	if (ActorDescContainerCollection.Num() <= 1)
	{
		return TArrayView<const UActorDescContainer*>();
	}

	uint32 NumExternalDataLayerContainers = ActorDescContainerCollection.Num() - ExternalDataLayerContainerStartIdx;
	return MakeArrayView(&ActorDescContainerCollection[ExternalDataLayerContainerStartIdx], NumExternalDataLayerContainers);
}

void FStreamingGenerationActorDescCollection::OnCollectionChanged()
{
	SortCollection();
}

void FStreamingGenerationActorDescCollection::SortCollection()
{
	if (IsEmpty())
	{
		return;
	}

	auto IsMainPartitionContainer = [this](const UActorDescContainer* ActorDescContainer)
	{
		bool bIsContentBundleContainer = ActorDescContainer->GetContentBundleGuid().IsValid() && GetActorDescContainerCount() == 1 && !ActorDescContainer->GetContainerPackage().ToString().StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase);
		return !ActorDescContainer->GetContentBundleGuid().IsValid() || bIsContentBundleContainer;
	};

	Algo::Sort(ActorDescContainerCollection, [&IsMainPartitionContainer](const UActorDescContainer* A, const UActorDescContainer* B)
	{
		if (IsMainPartitionContainer(A) && !IsMainPartitionContainer(B))
		{
			return true;
		}
		else if (IsMainPartitionContainer(B) && !IsMainPartitionContainer(A))
		{
			return false;
		}

		return A->GetContainerPackage().Compare(B->GetContainerPackage()) < 0;
	});

	check(IsMainPartitionContainer(ActorDescContainerCollection[MainContainerIdx]));

#if DO_CHECK
	TArrayView<const UActorDescContainer*> ExternalDataLayerContainerView = GetExternalDataLayerContainers();
	Algo::ForEach(ExternalDataLayerContainerView, [&IsMainPartitionContainer](const UActorDescContainer* ActorDescContainer) { check(!IsMainPartitionContainer(ActorDescContainer)); });
#endif
}

/* Deprecated */
void UWorldPartition::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler, const UActorDescContainer* ActorDescContainer, bool bEnableStreaming, bool)
{
	FCheckForErrorsParams Params;
	Params.ErrorHandler = ErrorHandler;
	Params.ActorDescContainer = ActorDescContainer;
	Params.bEnableStreaming = bEnableStreaming;

	CheckForErrors(Params);
}

void UWorldPartition::CheckForErrors(const FCheckForErrorsParams& Params)
{
	FActorDescList ModifiedActorDescList;

	FWorldPartitionStreamingGenerator::FWorldPartitionStreamingGeneratorParams StreamingGeneratorParams = FWorldPartitionStreamingGenerator::FWorldPartitionStreamingGeneratorParams()
		.SetWorldPartitionContext(Params.ActorDescContainer->GetWorldPartition())
		.SetModifiedActorsDescList(!Params.ActorDescContainer->IsTemplateContainer() ? &ModifiedActorDescList : nullptr)
		.SetErrorHandler(StreamingGenerationErrorHandlerOverride ? (*StreamingGenerationErrorHandlerOverride)(Params.ErrorHandler) : Params.ErrorHandler)
		.SetIsValidGrid([](FName GridName) { return true; })
		.SetEnableStreaming(Params.bEnableStreaming)
		.SetActorGuidsToContainerMap(Params.ActorGuidsToContainerMap);

	FWorldPartitionStreamingGenerator StreamingGenerator(StreamingGeneratorParams);

	FStreamingGenerationActorDescCollection Collection{Params.ActorDescContainer};
	StreamingGenerator.PreparationPhase(Collection);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
