// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationSharingModule.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimationSharingManager.h"

IMPLEMENT_MODULE( FAnimSharingModule, AnimationSharing);

TMap<const UWorld*, UAnimationSharingManager*> FAnimSharingModule::WorldAnimSharingManagers;

FOnAnimationSharingManagerCreated FAnimSharingModule::OnAnimationSharingManagerCreated;

void FAnimSharingModule::StartupModule()
{
	FWorldDelegates::OnPostWorldCleanup.AddStatic(&FAnimSharingModule::OnWorldCleanup);
}

void FAnimSharingModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<const UWorld*, UAnimationSharingManager*>& WorldAnimSharingManagerPair : WorldAnimSharingManagers)
	{
		Collector.AddReferencedObject(WorldAnimSharingManagerPair.Value, WorldAnimSharingManagerPair.Key);
	}

#if DEBUG_MATERIALS 
	for (UMaterialInterface* Material : UAnimationSharingManager::DebugMaterials)
	{
		Collector.AddReferencedObject(Material);
	}
#endif
}

bool FAnimSharingModule::CreateAnimationSharingManager(UWorld* InWorld, const UAnimationSharingSetup* Setup)
{
	if (InWorld && InWorld->IsGameWorld() && Setup && UAnimationSharingManager::AnimationSharingEnabled() && !WorldAnimSharingManagers.Contains(InWorld))
	{
		UE_SCOPED_ENGINE_ACTIVITY(TEXT("Initializing Animation Sharing"));
		UAnimationSharingManager* Manager = NewObject<UAnimationSharingManager>(InWorld);
		Manager->Initialise(Setup);
		WorldAnimSharingManagers.Add(InWorld, Manager);
		
		OnAnimationSharingManagerCreated.Broadcast(Manager, InWorld);

		return true;
	}

	return false;
}

void FAnimSharingModule::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{	
	WorldAnimSharingManagers.Remove(World);
}

