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

UObject* UInterchangeLightActorFactory::ProcessActor(AActor& SpawnedActor, const UInterchangeActorFactoryNode& FactoryNode, const UInterchangeBaseNodeContainer& NodeContainer)
{
	if (ALight* LightActor = Cast<ALight>(&SpawnedActor))
	{
		if (ULightComponent* LightComponent = LightActor->GetLightComponent())
		{
			LightComponent->UnregisterComponent();

			if (const UInterchangeLightFactoryNode* LightFactoryNode = Cast<UInterchangeLightFactoryNode>(&FactoryNode))
			{
				if (FString IESTexture; LightFactoryNode->GetCustomIESTexture(IESTexture))
				{
					IESTexture = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(IESTexture);
					if (const UInterchangeTextureLightProfileFactoryNode* TextureFactoryNode = Cast<UInterchangeTextureLightProfileFactoryNode>(NodeContainer.GetNode(IESTexture)))
					{
						FSoftObjectPath ReferenceObject;
						TextureFactoryNode->GetCustomReferenceObject(ReferenceObject);
						if (UTextureLightProfile* Texture = Cast<UTextureLightProfile>(ReferenceObject.TryLoad()))
						{
							LightComponent->SetIESTexture(Texture);
						}
					}
				}
			}

			return LightComponent;
		}
	}

	return nullptr;
}
