// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Scene/InterchangeActorFactory.h"

#include "InterchangeActorFactoryNode.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Scene/InterchangeActorHelper.h"

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeActorFactory)

UClass* UInterchangeActorFactory::GetFactoryClass() const
{
	return AActor::StaticClass();
}

UObject* UInterchangeActorFactory::ImportSceneObject_GameThread(const UInterchangeFactoryBase::FImportSceneObjectsParams& CreateSceneObjectsParams)
{
	using namespace UE::Interchange;

	UInterchangeActorFactoryNode* FactoryNode = Cast<UInterchangeActorFactoryNode>(CreateSceneObjectsParams.FactoryNode);
	if (!ensure(FactoryNode) || !CreateSceneObjectsParams.NodeContainer)
	{
		return nullptr;
	}

	AActor* SpawnedActor = ActorHelper::SpawnFactoryActor(CreateSceneObjectsParams);

	if (SpawnedActor)
	{
		if (UObject* ObjectToUpdate = ProcessActor(*SpawnedActor, *FactoryNode, *CreateSceneObjectsParams.NodeContainer, CreateSceneObjectsParams))
		{
			if (USceneComponent* RootComponent = SpawnedActor->GetRootComponent())
			{
				// Cache mobility value to allow application of transform
				EComponentMobility::Type CachedMobility = RootComponent->Mobility;
				RootComponent->SetMobility(EComponentMobility::Type::Movable);

				ActorHelper::ApplyAllCustomAttributes(CreateSceneObjectsParams, *ObjectToUpdate);

				// Restore mobility value
				if (CachedMobility != EComponentMobility::Type::Movable)
				{
					RootComponent->SetMobility(CachedMobility);
				}
			}
		}
	}

	return SpawnedActor;
}

UObject* UInterchangeActorFactory::ProcessActor(AActor& SpawnedActor, const UInterchangeActorFactoryNode& /*FactoryNode*/, const UInterchangeBaseNodeContainer& /*NodeContainer*/, const FImportSceneObjectsParams& Params)
{
	return SpawnedActor.GetRootComponent();
}
