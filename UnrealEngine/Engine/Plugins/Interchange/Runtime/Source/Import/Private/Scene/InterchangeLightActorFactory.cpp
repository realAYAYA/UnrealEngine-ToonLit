// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scene/InterchangeLightActorFactory.h"
#include "Scene/InterchangeActorHelper.h"
#include "Components/LightComponent.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "InterchangeTextureLightProfileFactoryNode.h"

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "InterchangeLightFactoryNode.h"
#include "Engine/Light.h"

UClass* UInterchangeLightActorFactory::GetFactoryClass() const
{
	return ALight::StaticClass();
}

UObject* UInterchangeLightActorFactory::CreateSceneObject(const UInterchangeFactoryBase::FCreateSceneObjectsParams& CreateSceneObjectsParams)
{
	ALight * SpawnedActor = Cast<ALight>(UE::Interchange::ActorHelper::SpawnFactoryActor(CreateSceneObjectsParams));

	if(!SpawnedActor)
	{
		return nullptr;
	}

	if(ULightComponent* LightComponent = SpawnedActor->GetLightComponent())
	{
		LightComponent->UnregisterComponent();
		if(const UInterchangeLightFactoryNode* LightFactoryNode = Cast<UInterchangeLightFactoryNode>(CreateSceneObjectsParams.FactoryNode))
		{
			if(FString IESTexture; LightFactoryNode->GetCustomIESTexture(IESTexture))
			{
				IESTexture = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(IESTexture);
				if(const UInterchangeTextureLightProfileFactoryNode* TextureFactoryNode = Cast<UInterchangeTextureLightProfileFactoryNode>(CreateSceneObjectsParams.NodeContainer->GetNode(IESTexture)))
				{
					FSoftObjectPath ReferenceObject;
					TextureFactoryNode->GetCustomReferenceObject(ReferenceObject);
					if(UTextureLightProfile* Texture = Cast<UTextureLightProfile>(ReferenceObject.TryLoad()))
					{
						LightComponent->SetIESTexture(Texture);
					}
				}
			}
		}
		CreateSceneObjectsParams.FactoryNode->ApplyAllCustomAttributeToObject(LightComponent);
	}

	return SpawnedActor;
}
