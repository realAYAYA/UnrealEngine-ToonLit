// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Scene/InterchangeStaticMeshActorFactory.h"

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

UObject* UInterchangeStaticMeshActorFactory::ProcessActor(AActor& SpawnedActor, const UInterchangeActorFactoryNode& FactoryNode, const UInterchangeBaseNodeContainer& NodeContainer, const FImportSceneObjectsParams& /*Params*/)
{
	using namespace UE::Interchange;

	AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(&SpawnedActor);

	if (!StaticMeshActor)
	{
		return nullptr;
	}

	if (UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent())
	{
		bool bHasGeometricTransform = false;
		FTransform GeometricTransform;
		const UInterchangeMeshActorFactoryNode* MeshActorFactoryNode = Cast<UInterchangeMeshActorFactoryNode>(&FactoryNode);
		if (MeshActorFactoryNode)
		{
			if (MeshActorFactoryNode->GetCustomGeometricTransform(GeometricTransform))
			{
				bHasGeometricTransform = true;
			}
		}


		if (bHasGeometricTransform)
		{
			UStaticMeshComponent* GeometricTransformMeshComponent = nullptr;
			if (StaticMeshComponent->GetNumChildrenComponents() > 0)
			{
				USceneComponent* ChildSceneComponent = StaticMeshComponent->GetChildComponent(0);
				if (ChildSceneComponent->IsA(UStaticMeshComponent::StaticClass()))
				{
					AActor* ParentActor = ChildSceneComponent->GetAttachParentActor();
					if (ParentActor == StaticMeshActor)
					{
						GeometricTransformMeshComponent = Cast<UStaticMeshComponent>(ChildSceneComponent);
					}
				}
			}

			if (GeometricTransformMeshComponent == nullptr)
			{
				StaticMeshActor->UnregisterAllComponents();

				GeometricTransformMeshComponent = NewObject<UStaticMeshComponent>(StaticMeshActor->GetRootComponent(), TEXT("GeometricTransform"));

#if WITH_EDITORONLY_DATA
				GeometricTransformMeshComponent->bVisualizeComponent = true;
#endif
				StaticMeshActor->AddInstanceComponent(GeometricTransformMeshComponent);

				if (const UInterchangeFactoryBaseNode* MeshNode = ActorHelper::FindAssetInstanceFactoryNode(&NodeContainer, &FactoryNode))
				{
					FSoftObjectPath ReferenceObject;
					MeshNode->GetCustomReferenceObject(ReferenceObject);
					if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ReferenceObject.TryLoad()))
					{
						if (StaticMesh != GeometricTransformMeshComponent->GetStaticMesh())
						{
							GeometricTransformMeshComponent->SetStaticMesh(StaticMesh);
						}
						if (MeshActorFactoryNode)
						{
							UE::Interchange::ActorHelper::ApplySlotMaterialDependencies(NodeContainer, *MeshActorFactoryNode, *GeometricTransformMeshComponent);
						}
					}
				}

				GeometricTransformMeshComponent->SetupAttachment(StaticMeshComponent);

				StaticMeshActor->RegisterAllComponents();

				EComponentMobility::Type MobilityToSet = StaticMeshComponent->Mobility;
				GeometricTransformMeshComponent->SetMobility(EComponentMobility::Type::Movable); //so that RelativeTransform can be set
				GeometricTransformMeshComponent->SetRelativeTransform(GeometricTransform);
				GeometricTransformMeshComponent->SetMobility(MobilityToSet);

				return StaticMeshComponent;
			}
		}

		if (const UInterchangeFactoryBaseNode* MeshNode = ActorHelper::FindAssetInstanceFactoryNode(&NodeContainer, &FactoryNode))
		{
			StaticMeshComponent->UnregisterComponent();

			FSoftObjectPath ReferenceObject;
			MeshNode->GetCustomReferenceObject(ReferenceObject);
			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ReferenceObject.TryLoad()))
			{
				if (StaticMesh != StaticMeshComponent->GetStaticMesh())
				{
					StaticMeshComponent->SetStaticMesh(StaticMesh);
				}
				if (MeshActorFactoryNode)
				{
					UE::Interchange::ActorHelper::ApplySlotMaterialDependencies(NodeContainer, *MeshActorFactoryNode, *StaticMeshComponent);
				}
			}

			StaticMeshComponent->RegisterComponent();
		}
		else
		{
			// TODO: Warn that new mesh has not been applied
		}

		return StaticMeshComponent;
	}

	return nullptr;
};
