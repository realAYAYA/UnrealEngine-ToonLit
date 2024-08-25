// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/AvaRundownPageAssetUtils.h"

#include "AvaScene.h"
#include "AvaTagHandle.h"
#include "AvaTransitionSubsystem.h"
#include "AvaTransitionTree.h"
#include "Behavior/IAvaTransitionBehavior.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "IAvaSceneInterface.h"

namespace UE::AvaRundownPageAssetUtils::Private
{
	template<typename InInterfaceType>
	const InInterfaceType* GetInterfaceFromAsset(const UObject* InLoadedAsset)
	{
		if (const InInterfaceType* const Interface = Cast<InInterfaceType>(InLoadedAsset))
		{
			return Interface;
		}
		if (const UWorld* const World = Cast<UWorld>(InLoadedAsset))
		{
			if (const AAvaScene* AvaScene = AAvaScene::GetScene(World->PersistentLevel, false))
			{
				return static_cast<const InInterfaceType*>(AvaScene);
			}
		}
		return nullptr;
	}
}

const IAvaSceneInterface* FAvaRundownPageAssetUtils::GetSceneInterface(const UObject* InLoadedAsset)
{
	using namespace UE::AvaRundownPageAssetUtils::Private;
	return GetInterfaceFromAsset<IAvaSceneInterface>(InLoadedAsset);
}

const UAvaTransitionTree* FAvaRundownPageAssetUtils::FindTransitionTree(const IAvaSceneInterface* InSceneInterface)
{
	if (!InSceneInterface)
	{
		return nullptr;
	}
	
	ULevel* SceneLevel = InSceneInterface->GetSceneLevel();
	if (!SceneLevel)
	{
		return nullptr;
	}

	// Method 1 - Using the UAvaTransitionSubsystem
	// This method may not work if the world is not initialized (inactive, as in the managed instance).
	if (const UWorld* SceneWorld = SceneLevel->GetWorld())
	{
		if (const UAvaTransitionSubsystem* TransitionSubsystem = SceneWorld->GetSubsystem<UAvaTransitionSubsystem>())
		{
			if (const IAvaTransitionBehavior* TransitionBehavior = TransitionSubsystem->GetTransitionBehavior(SceneLevel))
			{
				if (const UAvaTransitionTree* TransitionTree = TransitionBehavior->GetTransitionTree())
				{
					return TransitionTree;
				}
			}
			// If we had a subsystem and it didn't find the behavior then there was no state tree either and we are done.
			return nullptr;
		}
	}
	
	// Method 2 - Fallback using direct lookup of the private actor class.
	// This is needed to lookup in managed instances and source asset.

	static FString AvaTransitionBehaviorActor(TEXT("AvaTransitionBehaviorActor"));
	for (const AActor* Actor : SceneLevel->Actors)
	{
		const UClass* ActorClass = Actor->GetClass();
		if (ActorClass && ActorClass->GetName() == AvaTransitionBehaviorActor)
		{
			const FObjectPropertyBase* const Property = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("TransitionTree"));
			if (Property
				&& Property->PropertyClass
				&& Property->PropertyClass->IsChildOf(UAvaTransitionTree::StaticClass()))
			{
				return Cast<UAvaTransitionTree>(Property->GetObjectPropertyValue_InContainer(Actor));
			}
		}
	}
	return nullptr;
}

FAvaTagHandle FAvaRundownPageAssetUtils::GetTransitionLayerTag(const UAvaTransitionTree* InTransitionTree)
{
	return InTransitionTree ? InTransitionTree->GetTransitionLayer() : FAvaTagHandle();
}