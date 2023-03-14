// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/LightWeightInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"

DEFINE_LOG_CATEGORY(LogLightWeightInstance);

TSharedPtr<FLightWeightInstanceSubsystem> FLightWeightInstanceSubsystem::LWISubsystem;
FCriticalSection FLightWeightInstanceSubsystem::GetFunctionCS;

FLightWeightInstanceSubsystem::FLightWeightInstanceSubsystem()
{
#if WITH_EDITOR
	if (GEngine)
	{
		OnLevelActorAddedHandle = GEngine->OnLevelActorAdded().AddLambda([this](AActor* Actor)
			{
				if (ALightWeightInstanceManager* LWIManager = Cast<ALightWeightInstanceManager>(Actor))
				{
					FLightWeightInstanceSubsystem::Get().LWInstanceManagers.AddUnique(LWIManager);
				}
			});
		OnLevelActorDeletedHandle = GEngine->OnLevelActorDeleted().AddLambda([this](AActor* Actor)
			{
				if (ALightWeightInstanceManager* LWIManager = Cast<ALightWeightInstanceManager>(Actor))
				{
					FLightWeightInstanceSubsystem::Get().LWInstanceManagers.Remove(LWIManager);
				}
			});
	}
#endif // WITH_EDITOR
}

FLightWeightInstanceSubsystem::~FLightWeightInstanceSubsystem()
{
#if WITH_EDITOR
	if (GEngine)
	{
		GEngine->OnLevelActorAdded().Remove(OnLevelActorAddedHandle);
		GEngine->OnLevelActorDeleted().Remove(OnLevelActorDeletedHandle);
	}
#endif // WITH_EDITOR
}

int32 FLightWeightInstanceSubsystem::GetManagerIndex(const ALightWeightInstanceManager* Manager) const
{
	for (int32 Idx = 0; Idx < LWInstanceManagers.Num(); ++Idx)
	{
		if (LWInstanceManagers[Idx] == Manager)
		{
			return Idx;
		}
	}

	return INDEX_NONE;
}

const ALightWeightInstanceManager* FLightWeightInstanceSubsystem::GetManagerAt(int32 Index) const
{
	return LWInstanceManagers.IsValidIndex(Index) ? LWInstanceManagers[Index] : nullptr;
}

ALightWeightInstanceManager* FLightWeightInstanceSubsystem::FindLightWeightInstanceManager(const FActorInstanceHandle& Handle) const
{
	if (Handle.Manager.IsValid())
	{
		return Handle.Manager.Get();
	}

	if (Handle.Actor.IsValid())
	{
		for (ALightWeightInstanceManager* LWInstance : LWInstanceManagers)
		{
			if (Handle.Actor->GetClass() == LWInstance->GetRepresentedClass())
			{
#if WITH_EDITOR
				// make sure the data layers match
				TArray<const UDataLayerInstance*> ManagerLayers = LWInstance->GetDataLayerInstances();
				const UDataLayerInstance* ManagerLayer = ManagerLayers.Num() > 0 ? ManagerLayers[0] : nullptr;

				TArray<const UDataLayerInstance*> ActorLayers = LWInstance->GetDataLayerInstances();
				const UDataLayerInstance* ActorLayer = ActorLayers.Num() > 0 ? ActorLayers[0] : nullptr;

				if (ManagerLayer == ActorLayer)
#endif // WITH_EDITOR
				{
					return LWInstance;
				}
			}
		}
	}

	return nullptr;
}

ALightWeightInstanceManager* FLightWeightInstanceSubsystem::FindLightWeightInstanceManager(UClass* ActorClass, const UDataLayerInstance* Layer) const
{
	if (ActorClass == nullptr)
	{
		return nullptr;
	}

	// see if we already have a match
	for (ALightWeightInstanceManager* Instance : LWInstanceManagers)
	{
		if (Instance->GetRepresentedClass() == ActorClass)
		{
#if WITH_EDITOR
			if (!Layer || (Instance->SupportsDataLayer() && Instance->ContainsDataLayer(Layer)))
#endif // WITH_EDITOR
			{
				return Instance;
			}
		}
	}

	return nullptr;
}

ALightWeightInstanceManager* FLightWeightInstanceSubsystem::FindOrAddLightWeightInstanceManager(UClass* ActorClass, const UDataLayerInstance* DataLayer, UWorld* World)
{
	if (ActorClass == nullptr || World == nullptr)
	{
		return nullptr;
	}
	
	// see if we already have a match
	for (ALightWeightInstanceManager* InstanceManager : LWInstanceManagers)
	{
		if (InstanceManager->GetRepresentedClass() == ActorClass)
		{
#if WITH_EDITOR
			if (!DataLayer || (InstanceManager->SupportsDataLayer() && InstanceManager->ContainsDataLayer(DataLayer)))
#endif // WITH_EDITOR
			{
				return InstanceManager;
			}
		}
	}

	// we didn't find a match so we should add one.

	// Find the best base class to start from
	UClass* BestMatchingClass = FindBestInstanceManagerClass(ActorClass);
	if (BestMatchingClass == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags = RF_Transactional;

	ALightWeightInstanceManager* NewInstanceManager = World->SpawnActor<ALightWeightInstanceManager>(BestMatchingClass, FTransform::Identity, SpawnParams);
	NewInstanceManager->SetRepresentedClass(ActorClass);

#if WITH_EDITOR
	// Add the new manager to the DataLayer
	if (DataLayer)
	{
		ensure(NewInstanceManager->SupportsDataLayer());

		NewInstanceManager->AddDataLayer(DataLayer);
	}
#endif // WITH_EDITOR

	check(LWInstanceManagers.Find(NewInstanceManager) != INDEX_NONE);

	return NewInstanceManager;
}

UClass* FLightWeightInstanceSubsystem::FindBestInstanceManagerClass(const UClass* InActorClass)
{
	// Get every light weight instance class
	// FRED_TODO: we need to search through unloaded blueprints as well
	// FRED_TODO: this should be done once and cached. In the editor we can add a listener for when new BP classes are added
	TArray<UClass*> ManagerClasses;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(ALightWeightInstanceManager::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
		{
			// Skip SKEL and REINST classes.
			if (It->GetName().StartsWith(TEXT("SKEL_")) || It->GetName().StartsWith(TEXT("REINST_")))
			{
				continue;
			}
			ManagerClasses.Add(*It);
		}
	}

	// Figure out which one is the closest fit for InActorClass
	UClass* BestManagerClass = nullptr;
	int32 BestDistance = INT_MAX;
	for (UClass* ManagerClass : ManagerClasses)
	{
		if (ManagerClass->GetDefaultObject<ALightWeightInstanceManager>()->DoesAcceptClass(InActorClass))
		{
			const UClass* HandledClass = ManagerClass->GetDefaultObject<ALightWeightInstanceManager>()->GetRepresentedClass();
			if (!HandledClass)
			{
				HandledClass = ManagerClass->GetDefaultObject<ALightWeightInstanceManager>()->GetAcceptedClass();
			}
			if (InActorClass == HandledClass)
			{
				BestManagerClass = ManagerClass;
				break;
			}
			const UClass* ActorClass = InActorClass;
			int32 Distance = 0;
			while (ActorClass && ActorClass != HandledClass)
			{
				++Distance;
				ActorClass = ActorClass->GetSuperClass();
			}
			if (ActorClass && Distance < BestDistance)
			{
				BestDistance = Distance;
				BestManagerClass = ManagerClass;
			}
		}
	}

	return BestManagerClass;
}

AActor* FLightWeightInstanceSubsystem::FetchActor(const FActorInstanceHandle& Handle)
{
	// if the actor is valid return it
	if (Handle.Actor.IsValid())
	{
		return Handle.Actor.Get();
	}

	if (ALightWeightInstanceManager* LWIManager = FindLightWeightInstanceManager(Handle))
	{
		return LWIManager->FetchActorFromHandle(Handle);
	}

	return nullptr;
}

AActor* FLightWeightInstanceSubsystem::GetActor_NoCreate(const FActorInstanceHandle& Handle) const
{
	return Handle.Actor.Get();
}

UClass* FLightWeightInstanceSubsystem::GetActorClass(const FActorInstanceHandle& Handle)
{
	if (Handle.Actor.IsValid())
	{
		return Handle.Actor->StaticClass();
	}

	if (ALightWeightInstanceManager* LWIManager = FindLightWeightInstanceManager(Handle))
	{
		return LWIManager->GetRepresentedClass();
	}

	return nullptr;
}

FVector FLightWeightInstanceSubsystem::GetLocation(const FActorInstanceHandle& Handle)
{
	ensure(Handle.IsValid());

	if (Handle.Actor.IsValid())
	{
		return Handle.Actor->GetActorLocation();
	}

	if (ALightWeightInstanceManager* InstanceManager = FindLightWeightInstanceManager(Handle))
	{
		return InstanceManager->GetLocation(Handle);
	}

	return FVector::ZeroVector;
}

FString FLightWeightInstanceSubsystem::GetName(const FActorInstanceHandle& Handle)
{
	ensure(Handle.IsValid());

	if (Handle.Actor.IsValid())
	{
		return Handle.Actor->GetName();
	}

	if (ALightWeightInstanceManager* InstanceManager = FindLightWeightInstanceManager(Handle))
	{
		return InstanceManager->GetName(Handle);
	}

	return TEXT("None");
}

ULevel* FLightWeightInstanceSubsystem::GetLevel(const FActorInstanceHandle& Handle)
{
	ensure(Handle.IsValid());

	if (Handle.Actor.IsValid())
	{
		return Handle.Actor->GetLevel();
	}

	if (ALightWeightInstanceManager* InstanceManager = FindLightWeightInstanceManager(Handle))
	{
		return InstanceManager->GetLevel();
	}

	return nullptr;
}

bool FLightWeightInstanceSubsystem::IsInLevel(const FActorInstanceHandle& Handle, const ULevel* InLevel)
{
	ensure(Handle.IsValid());

	if (Handle.Actor.IsValid())
	{
		return Handle.Actor->IsInLevel(InLevel);
	}

	if (ALightWeightInstanceManager* InstanceManager = FindLightWeightInstanceManager(Handle))
	{
		return InstanceManager->GetLevel() == InLevel;
	}

	return false;
}

FActorInstanceHandle FLightWeightInstanceSubsystem::CreateNewLightWeightInstance(UClass* InActorClass, FLWIData* InitData, UDataLayerInstance* InLayer, UWorld* World)
{
	// Get or create a light weight instance for this class and data layer
	if (ALightWeightInstanceManager* LWIManager = FindOrAddLightWeightInstanceManager(InActorClass, InLayer, World))
	{
		// create an instance with the given data
		int32 InstanceIdx = LWIManager->AddNewInstance(InitData);
		InstanceIdx = LWIManager->ConvertInternalIndexToHandleIndex(InstanceIdx);
		return FActorInstanceHandle(LWIManager, InstanceIdx);
	}

	return FActorInstanceHandle();
}

void FLightWeightInstanceSubsystem::DeleteInstance(const FActorInstanceHandle& Handle)
{
	if (ALightWeightInstanceManager* LWIManager = FindLightWeightInstanceManager(Handle))
	{
		LWIManager->RemoveInstance(Handle.GetInstanceIndex());
	}
}
