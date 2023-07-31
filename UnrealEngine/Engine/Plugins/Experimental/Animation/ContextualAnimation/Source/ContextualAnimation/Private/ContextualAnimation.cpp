// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimation.h"
#include "ContextualAnimManager.h"

#define LOCTEXT_NAMESPACE "ContextualAnimationModule"

TMap<const UWorld*, UContextualAnimManager*> FContextualAnimationModule::WorldToManagerMap;

void FContextualAnimationModule::StartupModule()
{
	OnPreWorldInitDelegateHandle = FWorldDelegates::OnPreWorldInitialization.AddStatic(&FContextualAnimationModule::OnWorldInit);
	OnPostWorldCleanupDelegateHandle = FWorldDelegates::OnPostWorldCleanup.AddStatic(&FContextualAnimationModule::OnWorldCleanup);
}

void FContextualAnimationModule::ShutdownModule()
{
	FWorldDelegates::OnPreWorldInitialization.Remove(OnPreWorldInitDelegateHandle);
	FWorldDelegates::OnPostWorldCleanup.Remove(OnPostWorldCleanupDelegateHandle);

	WorldToManagerMap.Empty();
}

void FContextualAnimationModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<const UWorld*, UContextualAnimManager*>& Pair : WorldToManagerMap)
	{
		Collector.AddReferencedObject(Pair.Value, Pair.Key);
	}
}

void FContextualAnimationModule::OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	if(World && World->IsGameWorld())
	{
		WorldToManagerMap.Add(World, NewObject<UContextualAnimManager>(World));
	}
}

void FContextualAnimationModule::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	WorldToManagerMap.Remove(World);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FContextualAnimationModule, ContextualAnimation)