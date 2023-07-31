// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimManager.h"
#include "ContextualAnimSceneInstance.h"
#include "ContextualAnimSceneAsset.h"
#include "ContextualAnimation.h"
#include "ContextualAnimSceneActorComponent.h"
#include "ContextualAnimUtilities.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContextualAnimManager)

DECLARE_CYCLE_STAT(TEXT("ContextualAnim FindClosestSceneActorComp"), STAT_ContextualAnim_FindClosestSceneActorComp, STATGROUP_Anim);

UContextualAnimManager::UContextualAnimManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UContextualAnimManager* UContextualAnimManager::Get(const UWorld* World)
{
	return (World) ? FContextualAnimationModule::GetManager(World) : nullptr;
}

UContextualAnimManager* UContextualAnimManager::GetContextualAnimManager(UObject* WorldContextObject)
{
	return Get(GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull));
}

UWorld* UContextualAnimManager::GetWorld()const
{
	return CastChecked<UWorld>(GetOuter());
}

ETickableTickType UContextualAnimManager::GetTickableTickType() const
{
	//@TODO: Switch to Conditional and use IsTickable to determine whether to tick. It should only tick when scene instances are active
	return (HasAnyFlags(RF_ClassDefaultObject)) ? ETickableTickType::Never : ETickableTickType::Always;
}
TStatId UContextualAnimManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UContextualAnimManager, STATGROUP_Tickables);
}

bool UContextualAnimManager::IsTickableInEditor() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && !World->IsGameWorld();
}

void UContextualAnimManager::RegisterSceneActorComponent(UContextualAnimSceneActorComponent* SceneActorComp)
{
	if(SceneActorComp)
	{
		SceneActorCompContainer.Add(SceneActorComp);
	}
}

void UContextualAnimManager::UnregisterSceneActorComponent(UContextualAnimSceneActorComponent* SceneActorComp)
{
	if (SceneActorComp)
	{
		SceneActorCompContainer.Remove(SceneActorComp);
	}
}

void UContextualAnimManager::Tick(float DeltaTime)
{
	// Keep local copy since an instance might get unregistered by its tick
	TArray<UContextualAnimSceneInstance*> InstancesToTick = Instances;
	for (UContextualAnimSceneInstance* SceneInstance : InstancesToTick)
	{
		SceneInstance->Tick(DeltaTime);
	}
}

bool UContextualAnimManager::IsActorInAnyScene(AActor* Actor) const
{
	if (Actor)
	{
		for (const UContextualAnimSceneInstance* SceneInstance : Instances)
		{
			if (SceneInstance->IsActorInThisScene(Actor))
			{
				return true;
			}
		}
	}

	return false;
}

UContextualAnimSceneInstance* UContextualAnimManager::GetSceneWithActor(AActor* Actor)
{
	if (Actor)
	{
		for (UContextualAnimSceneInstance* SceneInstance : Instances)
		{
			if (SceneInstance->IsActorInThisScene(Actor))
			{
				return SceneInstance;
			}
		}
	}

	return nullptr;
}

UContextualAnimSceneInstance* UContextualAnimManager::ForceStartScene(const UContextualAnimSceneAsset& SceneAsset, const FContextualAnimStartSceneParams& Params)
{
	if(Params.RoleToActorMap.Num() == 0)
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimManager::ForceStartScene. Can't start scene. Reason: Empty Params. SceneAsset: %s"), *GetNameSafe(&SceneAsset));
		return nullptr;
	}

	const int32 SectionIdx = Params.SectionIdx;
	const int32 AnimSetIdx = Params.AnimSetIdx;
	FContextualAnimSceneBindings Bindings = FContextualAnimSceneBindings(SceneAsset, SectionIdx, AnimSetIdx);

	for (const auto& Pair : Params.RoleToActorMap)
	{
		FName RoleToBind = Pair.Key;

		const AActor* ActorToBind = Pair.Value.GetActor();
		if (ActorToBind == nullptr)
		{
			UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimManager::ForceStartScene. Can't start scene. Reason: Trying to bind Invalid Actor. SceneAsset: %s Role: %s"),
				*GetNameSafe(&SceneAsset), *RoleToBind.ToString());

			return nullptr;
		}

		const FContextualAnimTrack* AnimTrack = SceneAsset.GetAnimTrack(SectionIdx, AnimSetIdx, RoleToBind);
		if (AnimTrack == nullptr)
		{
			UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimManager::ForceStartScene. Can't start scene. Reason: Can't find anim track for '%s'. SceneAsset: %s"), 
				*RoleToBind.ToString(), *GetNameSafe(&SceneAsset));

			return nullptr;
		}

		Bindings.Add(FContextualAnimSceneBinding(Pair.Value, *AnimTrack));
	}

	const UClass* Class = SceneAsset.GetSceneInstanceClass();
	UContextualAnimSceneInstance* NewInstance = Class ? NewObject<UContextualAnimSceneInstance>(this, Class) : NewObject<UContextualAnimSceneInstance>(this);
	NewInstance->SceneAsset = &SceneAsset;
	Bindings.SceneInstancePtr = NewInstance;

	if (Params.Pivots.IsEmpty())
	{
		Bindings.CalculateAnimSetPivots(NewInstance->GetMutablePivots());
	}
	else
	{
		NewInstance->SetPivots(Params.Pivots);
	}

	NewInstance->Bindings = MoveTemp(Bindings);
	NewInstance->Start();
	NewInstance->OnSceneEnded.AddDynamic(this, &UContextualAnimManager::OnSceneInstanceEnded);

	Instances.Add(NewInstance);

	return NewInstance;
}

UContextualAnimSceneInstance* UContextualAnimManager::BP_TryStartScene(const UContextualAnimSceneAsset* SceneAsset, const FContextualAnimStartSceneParams& Params)
{
	if(SceneAsset == nullptr)
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimManager::TryStartScene. Can't start scene. Reason: Invalid Scene Asset"));
		return nullptr;
	}

	return TryStartScene(*SceneAsset, Params);
}

UContextualAnimSceneInstance* UContextualAnimManager::TryStartScene(const UContextualAnimSceneAsset& SceneAsset, const FContextualAnimStartSceneParams& Params)
{
	const int32 SectionIdx = Params.SectionIdx;
	FContextualAnimSceneBindings Bindings;

	bool bSuccess = false;
	if (Params.AnimSetIdx != INDEX_NONE)
	{
		bSuccess = FContextualAnimSceneBindings::TryCreateBindings(SceneAsset, SectionIdx, Params.AnimSetIdx, Params.RoleToActorMap, Bindings);
	}
	else
	{
		const int32 NumSets = SceneAsset.GetNumAnimSetsInSection(SectionIdx);
		for (int32 AnimSetIdx = 0; AnimSetIdx < NumSets; AnimSetIdx++)
		{
			if (FContextualAnimSceneBindings::TryCreateBindings(SceneAsset, SectionIdx, AnimSetIdx, Params.RoleToActorMap, Bindings))
			{
				bSuccess = true;
				break;
			}
		}
	}

	if (bSuccess)
	{
		const UClass* Class = SceneAsset.GetSceneInstanceClass();
		UContextualAnimSceneInstance* NewInstance = Class ? NewObject<UContextualAnimSceneInstance>(this, Class) : NewObject<UContextualAnimSceneInstance>(this);
		NewInstance->SceneAsset = &SceneAsset;
		Bindings.SceneInstancePtr = NewInstance;

		if (Params.Pivots.IsEmpty())
		{
			Bindings.CalculateAnimSetPivots(NewInstance->GetMutablePivots());
		}
		else
		{
			NewInstance->SetPivots(Params.Pivots);
		}

		NewInstance->Bindings = MoveTemp(Bindings);
		NewInstance->Start();
		NewInstance->OnSceneEnded.AddDynamic(this, &UContextualAnimManager::OnSceneInstanceEnded);

		Instances.Add(NewInstance);

		return NewInstance;
	}

	return nullptr;
}

bool UContextualAnimManager::TryStopSceneWithActor(AActor* Actor)
{
	if(UContextualAnimSceneInstance* SceneInstance = GetSceneWithActor(Actor))
	{
		SceneInstance->Stop();
		return true;
	}

	return false;
}

void UContextualAnimManager::OnSceneInstanceEnded(UContextualAnimSceneInstance* SceneInstance)
{
	if(SceneInstance)
	{
		Instances.Remove(SceneInstance);
	}
}
