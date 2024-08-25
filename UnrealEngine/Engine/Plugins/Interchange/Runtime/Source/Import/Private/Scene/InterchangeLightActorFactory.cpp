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

UObject* UInterchangeLightActorFactory::ProcessActor(AActor& SpawnedActor, const UInterchangeActorFactoryNode& FactoryNode, const UInterchangeBaseNodeContainer& NodeContainer, const FImportSceneObjectsParams& /*Params*/)
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
					FString IESFactoryTextureId = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(IESTexture);
					if (const UInterchangeTextureLightProfileFactoryNode* TextureFactoryNode = Cast<UInterchangeTextureLightProfileFactoryNode>(NodeContainer.GetNode(IESFactoryTextureId)))
					{
						FSoftObjectPath ReferenceObject;
						TextureFactoryNode->GetCustomReferenceObject(ReferenceObject);
						if (UTextureLightProfile* Texture = Cast<UTextureLightProfile>(ReferenceObject.TryLoad()))
						{
							LightComponent->SetIESTexture(Texture);
							
							if (bool bUseIESBrightness; LightFactoryNode->GetCustomUseIESBrightness(bUseIESBrightness))
							{
								LightComponent->SetUseIESBrightness(bUseIESBrightness);
							}
							
							if (float IESBrightnessScale; LightFactoryNode->GetCustomIESBrightnessScale(IESBrightnessScale))
							{
								LightComponent->SetIESBrightnessScale(IESBrightnessScale);
							}
							
							if (FRotator Rotation; LightFactoryNode->GetCustomRotation(Rotation))
							{
								// Apply Ies rotation to light orientation
								FQuat NewLightRotation = LightComponent->GetRelativeRotation().Quaternion() * Rotation.Quaternion();
								LightComponent->SetRelativeRotation(NewLightRotation);
							}
						}
					}
				}
			}

			return LightComponent;
		}
	}

	return nullptr;
}
