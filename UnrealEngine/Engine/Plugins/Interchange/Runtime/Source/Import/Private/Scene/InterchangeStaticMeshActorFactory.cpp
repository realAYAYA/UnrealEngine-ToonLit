// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Scene/InterchangeStaticMeshActorFactory.h"

#include "InterchangeActorFactoryNode.h"
#include "InterchangeMeshActorFactoryNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Scene/InterchangeActorHelper.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeStaticMeshActorFactory)

UClass* UInterchangeStaticMeshActorFactory::GetFactoryClass() const
{
	return AStaticMeshActor::StaticClass();
}

UObject* UInterchangeStaticMeshActorFactory::CreateSceneObject(const UInterchangeFactoryBase::FCreateSceneObjectsParams& CreateSceneObjectsParams)
{
	AStaticMeshActor* SpawnedActor = Cast<AStaticMeshActor>(UE::Interchange::ActorHelper::SpawnFactoryActor(CreateSceneObjectsParams));

	if (!SpawnedActor)
	{
		return nullptr;
	}

	UInterchangeFactoryBaseNode* FactoryNode = CreateSceneObjectsParams.FactoryNode;
	SetupStaticMeshActor(CreateSceneObjectsParams.NodeContainer, FactoryNode, SpawnedActor);

	if (UStaticMeshComponent* StaticMeshComponent = SpawnedActor->GetStaticMeshComponent())
	{
		FactoryNode->ApplyAllCustomAttributeToObject(StaticMeshComponent);
	}

	return SpawnedActor;
};

void UInterchangeStaticMeshActorFactory::SetupStaticMeshActor(const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeFactoryBaseNode* ActorFactoryNode, AStaticMeshActor* StaticMeshActor)
{
	if (!StaticMeshActor)
	{
		return;
	}

	if (UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent())
	{
		StaticMeshComponent->UnregisterComponent();

		if (const UInterchangeFactoryBaseNode* MeshNode = UE::Interchange::ActorHelper::FindAssetInstanceFactoryNode(NodeContainer, ActorFactoryNode))
		{
			FSoftObjectPath ReferenceObject;
			MeshNode->GetCustomReferenceObject(ReferenceObject);
			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ReferenceObject.TryLoad()))
			{
				StaticMeshComponent->SetStaticMesh(StaticMesh);

				if (const UInterchangeMeshActorFactoryNode* MeshActorFactoryNode = Cast<UInterchangeMeshActorFactoryNode>(ActorFactoryNode))
				{
					UE::Interchange::ActorHelper::ApplySlotMaterialDependencies(*NodeContainer, *MeshActorFactoryNode, *StaticMeshComponent);
				}
			}
		}
	}
}
