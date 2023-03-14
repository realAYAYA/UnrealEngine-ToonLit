// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeDatasmithAreaLightFactory.h"

#include "InterchangeDatasmithLog.h"
#include "InterchangeDatasmithAreaLightFactoryNode.h"

#include "Scene/InterchangeActorHelper.h"
#include "DatasmithAreaLightActor.h"
#include "Engine/TextureLightProfile.h"
#include "InterchangeTextureFactoryNode.h"
#include "Logging/LogMacros.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

UClass* UInterchangeDatasmithAreaLightFactory::GetFactoryClass() const
{
	FSoftObjectPath LightShapeBlueprintRef = FSoftObjectPath(TEXT("/DatasmithContent/Datasmith/DatasmithArealight.DatasmithArealight"));
	UBlueprint* LightShapeBlueprint = Cast< UBlueprint >(LightShapeBlueprintRef.ResolveObject());

	// LightShapeBlueprint should already be loaded at this point.
	//if (!ensure(LightShapeBlueprint))
	if (!LightShapeBlueprint)
	{
		return nullptr;
	}

	return LightShapeBlueprint->GeneratedClass;
}

UObject* UInterchangeDatasmithAreaLightFactory::CreateSceneObject(const UInterchangeFactoryBase::FCreateSceneObjectsParams& CreateSceneObjectsParams)
{
	//using namespace UE::Interchange::ActorHelper;

	UInterchangeDatasmithAreaLightFactoryNode* AreaLightFactoryNode = Cast<UInterchangeDatasmithAreaLightFactoryNode>(CreateSceneObjectsParams.FactoryNode);
	if (!AreaLightFactoryNode)
	{
		return nullptr;
	}

	ADatasmithAreaLightActor* SpawnedAreaLightActor = Cast<ADatasmithAreaLightActor>(UE::Interchange::ActorHelper::SpawnFactoryActor(CreateSceneObjectsParams));
	if (!SpawnedAreaLightActor)
	{
		return nullptr;
	}

	// Update AreaLight properties and rebuild the actor.
	{
		SpawnedAreaLightActor->UnregisterAllComponents(true);

		// Find the IES Texture asset and apply it.
		FString IESTextureUid;
		if (AreaLightFactoryNode->GetCustomIESTexture(IESTextureUid))
		{
			const UInterchangeBaseNodeContainer* NodeContainer = CreateSceneObjectsParams.NodeContainer;

			if (const UInterchangeBaseNode* IESTextureNode = NodeContainer->GetNode(IESTextureUid))
			{
				TArray<FString> TargetNodesUid;
				IESTextureNode->GetTargetNodeUids(TargetNodesUid);
				for (const FString& TargetNodeUid : TargetNodesUid)
				{
					if (UInterchangeTextureFactoryNode* TextureFactoryNode = Cast<UInterchangeTextureFactoryNode>(NodeContainer->GetFactoryNode(TargetNodeUid)))
					{
						FSoftObjectPath TextureReferenceObject;
						TextureFactoryNode->GetCustomReferenceObject(TextureReferenceObject);
						if (UTextureLightProfile* TextureLightProfile = Cast<UTextureLightProfile>(TextureReferenceObject.TryLoad()))
						{
							SpawnedAreaLightActor->IESTexture = TextureLightProfile;
							break;
						}
					}
				}
			}
		}

		// Apply all simple attributes.
		AreaLightFactoryNode->ApplyAllCustomAttributeToObject(SpawnedAreaLightActor);

		SpawnedAreaLightActor->RegisterAllComponents();
#if WITH_EDITOR
		SpawnedAreaLightActor->RerunConstructionScripts();
#endif //WITH_EDITOR
	}

	return SpawnedAreaLightActor;
};