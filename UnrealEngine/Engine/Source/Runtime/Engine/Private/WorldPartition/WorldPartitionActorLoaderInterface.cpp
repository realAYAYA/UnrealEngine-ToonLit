// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionActorLoaderInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionActorLoaderInterface)

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "WorldPartition/WorldPartitionActorDescViewProxy.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#endif

#define LOCTEXT_NAMESPACE "WorldPartition"

UWorldPartitionActorLoaderInterface::UWorldPartitionActorLoaderInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

#if WITH_EDITOR
IWorldPartitionActorLoaderInterface::ILoaderAdapter::ILoaderAdapter(UWorld* InWorld)
	: World(InWorld)
	, bLoaded(false)
	, bUserCreated(false)
{
	check(!World->IsGameWorld());
}

IWorldPartitionActorLoaderInterface::ILoaderAdapter::~ILoaderAdapter()
{
	UnregisterDelegates();
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::Load()
{
	if (!bLoaded)
	{
		bLoaded = true;
		RefreshLoadedState();
		RegisterDelegates();

		if (bUserCreated)
		{
			if (UWorldPartition* WorldPartition = World->GetWorldPartition())
			{
				WorldPartition->OnUserCreatedRegionLoaded();
			}
		}
	}
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::Unload()
{
	if (bLoaded && !IsEngineExitRequested())
	{
		if (UWorldPartition* WorldPartition = World->GetWorldPartition())
		{
			UnregisterDelegates();

			FScopedSlowTask SlowTask(1, LOCTEXT("UpdatingLoading", "Updating loading..."));
			SlowTask.MakeDialogDelayed(1.0f);

			int32 NumUnloads = 0;
			{
				FWorldPartitionLoadingContext::FDeferred LoadingContext;
				ActorReferences.Empty();
				NumUnloads = LoadingContext.GetNumUnregistrations();
			}

			SlowTask.EnterProgressFrame(1);

			bLoaded = false;

			if (bUserCreated)
			{
				WorldPartition->OnUserCreatedRegionUnloaded();
			}

			if (NumUnloads)
			{
				PostLoadedStateChanged(0, NumUnloads);
			}
		}
	}
}

bool IWorldPartitionActorLoaderInterface::ILoaderAdapter::IsLoaded() const
{
	return bLoaded;
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::RegisterDelegates()
{
	FDataLayersEditorBroadcast::Get().OnActorDataLayersEditorLoadingStateChanged().AddRaw(this, &IWorldPartitionActorLoaderInterface::ILoaderAdapter::OnActorDataLayersEditorLoadingStateChanged);
	check(World->GetWorldPartition() != nullptr);
	if (UWorldPartition* WorldPartition = World->GetWorldPartition())
	{
		WorldPartition->OnActorDescContainerRegistered.AddRaw(this, &IWorldPartitionActorLoaderInterface::ILoaderAdapter::ILoaderAdapter::OnActorDescContainerInitialize);
		WorldPartition->OnActorDescContainerUnregistered.AddRaw(this, &IWorldPartitionActorLoaderInterface::ILoaderAdapter::ILoaderAdapter::OnActorDescContainerUninitialize);
	}
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::UnregisterDelegates()
{
	FDataLayersEditorBroadcast::Get().OnActorDataLayersEditorLoadingStateChanged().RemoveAll(this);
	check(World->GetWorldPartition() != nullptr);
	if (UWorldPartition* WorldPartition = World->GetWorldPartition())
	{
		WorldPartition->OnActorDescContainerRegistered.RemoveAll(this);
		WorldPartition->OnActorDescContainerUnregistered.RemoveAll(this);
	}
}

bool IWorldPartitionActorLoaderInterface::ILoaderAdapter::PassActorDescFilter(const FWorldPartitionHandle& Actor) const
{
	return !Actor->GetActorIsRuntimeOnly();
}

bool IWorldPartitionActorLoaderInterface::ILoaderAdapter::PassDataLayersFilter(const FWorldPartitionHandle& Actor) const
{
	UWorld* OwningWorld = World->PersistentLevel->GetWorld();
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(OwningWorld))
	{
		FWorldPartitionActorViewProxy ActorDescProxy(*Actor);

		if (IsRunningCookCommandlet())
		{
			// When running cook commandlet, dont allow loading of actors with runtime loaded data layers
			for (const FName& DataLayerInstanceName : ActorDescProxy.GetDataLayers())
			{
				const UDataLayerInstance* DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName);
				if (DataLayerInstance && DataLayerInstance->IsRuntime())
				{
					return false;
				}
			}
		}
		else
		{
			uint32 NumValidLayers = 0;
			for (const FName& DataLayerInstanceName : ActorDescProxy.GetDataLayers())
			{
				if (const UDataLayerInstance* DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName))
				{
					if (DataLayerInstance->IsEffectiveLoadedInEditor())
					{
						return true;
					}
					NumValidLayers++;
				}
			}
			return !NumValidLayers;
		}
	}

	return true;
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::RefreshLoadedState()
{
	if (bLoaded)
	{
		if (UWorldPartition* WorldPartition = World->GetWorldPartition())
		{
			TArray<FWorldPartitionHandle> ActorsToLoad;
			TArray<FWorldPartitionHandle> ActorsToUnload;			
			ForEachActor([this, &ActorsToLoad, &ActorsToUnload](const FWorldPartitionHandle& Actor)
			{
				if (ShouldActorBeLoaded(Actor))
				{
					if (!ActorReferences.Contains(Actor->GetGuid()))
					{
						ActorsToLoad.Add(Actor);
					}
				}
				else if (ActorReferences.Contains(Actor->GetGuid()))
				{
					ActorsToUnload.Add(Actor);
				}
			});

			if (ActorsToLoad.Num() || ActorsToUnload.Num())
			{
				int32 NumLoads = 0;
				int32 NumUnloads = 0;
				{
					FWorldPartitionLoadingContext::FDeferred LoadingContext;

					if (ActorsToLoad.Num())
					{
						FScopedSlowTask SlowTask(ActorsToLoad.Num(), LOCTEXT("ActorsLoading", "Loading actors..."));
						SlowTask.MakeDialogDelayed(1.0f);

						for (FWorldPartitionHandle& ActorToLoad : ActorsToLoad)
						{
							FWorldPartitionHandle ActorHandle(WorldPartition, ActorToLoad->GetGuid());
							AddReferenceToActor(ActorHandle);
							SlowTask.EnterProgressFrame(1);
						}
					}

					if (ActorsToUnload.Num())
					{
						FScopedSlowTask SlowTask(ActorsToUnload.Num(), LOCTEXT("ActorsUnoading", "Unloading actors..."));
						SlowTask.MakeDialogDelayed(1.0f);

						for (FWorldPartitionHandle& ActorToUnload : ActorsToUnload)
						{
							RemoveReferenceToActor(ActorToUnload);
							SlowTask.EnterProgressFrame(1);
						}
					}

					NumLoads = LoadingContext.GetNumRegistrations();
					NumUnloads = LoadingContext.GetNumUnregistrations();
				}

				if (NumLoads || NumUnloads)
				{
					PostLoadedStateChanged(NumLoads, NumUnloads);
				}
			}
		}
	}
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::OnActorDescContainerInitialize(UActorDescContainer* Container)
{
	RefreshLoadedState();
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::OnActorDescContainerUninitialize(UActorDescContainer* Container)
{
	// @todo_ow :
	// If the container is removed before broadcasting its uninitialize event we could simply call RefreshLoadedState(), but it would break call symetry
	TArray<FWorldPartitionHandle> ActorsToUnload;
	for(UActorDescContainer::TConstIterator<> It(Container); It; ++It)
	{
		if( ActorReferences.Find(It->GetGuid()))
		{
			ActorsToUnload.Emplace(Container, It->GetGuid());
		}
	}
	
	if (ActorsToUnload.Num())
	{
		FWorldPartitionLoadingContext::FDeferred LoadingContext;
	
		FScopedSlowTask SlowTask(ActorsToUnload.Num(), LOCTEXT("ActorsUnoading", "Unloading actors..."));
		SlowTask.MakeDialogDelayed(1.0f);
	
		for (FWorldPartitionHandle& ActorToUnload : ActorsToUnload)
		{
			RemoveReferenceToActor(ActorToUnload);
			SlowTask.EnterProgressFrame(1);
		}
	
		PostLoadedStateChanged(0, ActorsToUnload.Num());
	}
}

bool IWorldPartitionActorLoaderInterface::ILoaderAdapter::ShouldActorBeLoaded(const FWorldPartitionHandle& Actor) const
{
	check(Actor.IsValid());
	return PassActorDescFilter(Actor) && PassDataLayersFilter(Actor);
};

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::PostLoadedStateChanged(int32 NumLoads, int32 NumUnloads)
{
	check(NumLoads || NumUnloads);

	if (!IsRunningCommandlet())
	{
		if (NumUnloads)
		{
			GEditor->SelectNone(true, true);
		}

		GEngine->BroadcastLevelActorListChanged();
		GEditor->NoteSelectionChange();

		GEditor->ResetTransaction(LOCTEXT("LoadingEditorActorResetTrans", "Editor Actors Loading State Changed"));

		if (NumUnloads)
		{
			GEngine->ForceGarbageCollection(true);
		}
	}
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::AddReferenceToActor(FWorldPartitionHandle& ActorHandle)
{
	TFunction<void(const FWorldPartitionHandle&, TMap<FGuid, FWorldPartitionReference>&)> AddReferences = [this, &ActorHandle, &AddReferences](const FWorldPartitionHandle& Handle, TMap<FGuid, FWorldPartitionReference>& ReferenceMap)
	{
		if (!ReferenceMap.Contains(Handle->GetGuid()))
		{
			ReferenceMap.Emplace(Handle->GetGuid(), Handle.ToReference());
			
			for (const FGuid& ReferencedActorGuid : ActorHandle->GetReferences())
			{
				FWorldPartitionHandle ReferenceActorHandle(ActorHandle->GetContainer(), ReferencedActorGuid);

				if (ReferenceActorHandle.IsValid())
				{
					AddReferences(ReferenceActorHandle, ReferenceMap);
				}
			}
		}
	};

	AddReferences(ActorHandle, ActorReferences.Emplace(ActorHandle->GetGuid()));
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::RemoveReferenceToActor(FWorldPartitionHandle& ActorHandle)
{
	ActorReferences.Remove(ActorHandle->GetGuid());
}

void IWorldPartitionActorLoaderInterface::ILoaderAdapter::OnActorDataLayersEditorLoadingStateChanged(bool bFromUserOperation)
{
	RefreshLoadedState();
}
#endif

#undef LOCTEXT_NAMESPACE

