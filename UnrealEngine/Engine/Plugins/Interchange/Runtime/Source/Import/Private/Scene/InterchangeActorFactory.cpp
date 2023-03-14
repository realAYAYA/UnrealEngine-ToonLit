// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Scene/InterchangeActorFactory.h"

#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Scene/InterchangeActorHelper.h"

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeActorFactory)

UClass* UInterchangeActorFactory::GetFactoryClass() const
{
	return AActor::StaticClass();
}

UObject* UInterchangeActorFactory::CreateSceneObject(const UInterchangeFactoryBase::FCreateSceneObjectsParams& CreateSceneObjectsParams)
{
	AActor* SpawnedActor = UE::Interchange::ActorHelper::SpawnFactoryActor(CreateSceneObjectsParams);

	if (!SpawnedActor)
	{
		return nullptr;
	}

	if (USceneComponent* RootComponent = SpawnedActor->GetRootComponent())
	{
		CreateSceneObjectsParams.FactoryNode->ApplyAllCustomAttributeToObject(RootComponent);
	}

	return SpawnedActor;
}
