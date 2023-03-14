// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeGenericScenesPipeline.h"

#include "InterchangeActorFactoryNode.h"
#include "InterchangeCameraNode.h"
#include "InterchangeCineCameraFactoryNode.h"
#include "InterchangeCommonPipelineDataFactoryNode.h"
#include "InterchangeLightNode.h"
#include "InterchangeLightFactoryNode.h"
#include "InterchangeMeshActorFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSceneVariantSetsFactoryNode.h"
#include "InterchangeVariantSetNode.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "InterchangeSkeletonFactoryNode.h"

#include "Animation/SkeletalMeshActor.h"
#include "CineCameraActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMeshActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGenericScenesPipeline)

void UInterchangeGenericLevelPipeline::ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas)
{
	if (!InBaseNodeContainer)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericAssetsPipeline: Cannot execute pre-import pipeline because InBaseNodeContrainer is null"));
		return;
	}

	BaseNodeContainer = InBaseNodeContainer;

	FTransform GlobalOffsetTransform = FTransform::Identity;
	if (UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(BaseNodeContainer))
	{
		CommonPipelineDataFactoryNode->GetCustomGlobalOffsetTransform(GlobalOffsetTransform);
	}

	TArray<UInterchangeSceneNode*> SceneNodes;

	//Find all translated node we need for this pipeline
	BaseNodeContainer->IterateNodes([&SceneNodes](const FString& NodeUid, UInterchangeBaseNode* Node)
	{
		switch(Node->GetNodeContainerType())
		{
		case EInterchangeNodeContainerType::TranslatedScene:
		{
			if (UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node))
			{
				SceneNodes.Add(SceneNode);
			}
		}
		break;
		}
	});

	for (const UInterchangeSceneNode* SceneNode : SceneNodes)
	{
		if (SceneNode)
		{
			if (SceneNode->GetSpecializedTypeCount() > 0)
			{
				TArray<FString> SpecializeTypes;
				SceneNode->GetSpecializedTypes(SpecializeTypes);
				if (!SpecializeTypes.Contains(UE::Interchange::FSceneNodeStaticData::GetTransformSpecializeTypeString()))
				{
					bool bSkipNode = true;
					if (SpecializeTypes.Contains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
					{
						//check if its the rootjoint (we want to create an actor for the rootjoint)
						FString CurrentNodesParentUid = SceneNode->GetParentUid();
						const UInterchangeBaseNode* ParentNode = BaseNodeContainer->GetNode(CurrentNodesParentUid);
						if (const UInterchangeSceneNode* ParentSceneNode = Cast<UInterchangeSceneNode>(ParentNode))
						{
							if (!ParentSceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
							{
								bSkipNode = false;
							}
						}
					}

					if (bSkipNode)
					{
						//Skip any scene node that have specialized types but not the "Transform" type.
						continue;
					}
				}
			}
			ExecuteSceneNodePreImport(GlobalOffsetTransform, SceneNode);
		}
	}

	//Find all translated scene variant sets
	TArray<UInterchangeSceneVariantSetsNode*> SceneVariantSetNodes;

InBaseNodeContainer->IterateNodesOfType<UInterchangeSceneVariantSetsNode>([&SceneVariantSetNodes](const FString& NodeUid, UInterchangeSceneVariantSetsNode* Node)
	{
		SceneVariantSetNodes.Add(Node);
	});

for (const UInterchangeSceneVariantSetsNode* SceneVariantSetNode : SceneVariantSetNodes)
{
	if (SceneVariantSetNode)
	{
		ExecuteSceneVariantSetNodePreImport(*SceneVariantSetNode);
	}
}
}

void UInterchangeGenericLevelPipeline::ExecuteSceneNodePreImport(const FTransform& GlobalOffsetTransform, const UInterchangeSceneNode* SceneNode)
{
	if (!BaseNodeContainer || !SceneNode)
	{
		return;
	}

	const UInterchangeBaseNode* TranslatedAssetNode = nullptr;
	bool bRootJointNode = SceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
	FString SkeletalMeshFactoryNodeUid;

	if (bRootJointNode)
	{
		FString SkeletonFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(SceneNode->GetUniqueID());
		const UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = Cast<UInterchangeSkeletonFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletonFactoryNodeUid));
		if (SkeletonFactoryNode)
		{
			if (SkeletonFactoryNode->GetCustomSkeletalMeshFactoryNodeUid(SkeletalMeshFactoryNodeUid))
			{
				if (const UInterchangeFactoryBaseNode* SkeletalMeshFactoryNode = BaseNodeContainer->GetFactoryNode(SkeletalMeshFactoryNodeUid))
				{
					TArray<FString> NodeUids;
					SkeletalMeshFactoryNode->GetTargetNodeUids(NodeUids);

					if (NodeUids.Num() > 0)
					{
						TranslatedAssetNode = BaseNodeContainer->GetNode(NodeUids[0]);
					}
					else
					{
						TranslatedAssetNode = nullptr;
					}
				}
			}
		}
	}
	else
	{
		FString AssetInstanceUid;
		if (SceneNode->GetCustomAssetInstanceUid(AssetInstanceUid))
		{
			TranslatedAssetNode = BaseNodeContainer->GetNode(AssetInstanceUid);
		}
	}

	UInterchangeActorFactoryNode* ActorFactoryNode = CreateActorFactoryNode(SceneNode, TranslatedAssetNode);

	if (!ensure(ActorFactoryNode))
	{
		return;
	}

	FString NodeUid = SceneNode->GetUniqueID() + (bRootJointNode ? TEXT("_SkeletonNode") : TEXT(""));
	FString FactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(NodeUid);

	ActorFactoryNode->InitializeNode(FactoryNodeUid, SceneNode->GetDisplayLabel(), EInterchangeNodeContainerType::FactoryData);
	const FString ActorFactoryNodeUid = BaseNodeContainer->AddNode(ActorFactoryNode);
	if (!SceneNode->GetParentUid().IsEmpty())
	{
		const FString ParentFactoryNodeUid = TEXT("Factory_") + SceneNode->GetParentUid();
		BaseNodeContainer->SetNodeParentUid(ActorFactoryNodeUid, ParentFactoryNodeUid);
		ActorFactoryNode->AddFactoryDependencyUid(ParentFactoryNodeUid);
	}

	if (bRootJointNode)
	{
		ActorFactoryNode->AddTargetNodeUid(SkeletalMeshFactoryNodeUid);
	}
	else
	{
		ActorFactoryNode->AddTargetNodeUid(SceneNode->GetUniqueID());
		SceneNode->AddTargetNodeUid(ActorFactoryNode->GetUniqueID());
	}

	//TODO move this code to the factory, a stack over pipeline can change the global offset transform which will affect this value.
	FTransform GlobalTransform;
	if (SceneNode->GetCustomGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalTransform))
	{
		if (bRootJointNode)
		{
			GlobalTransform = FTransform::Identity;
			//LocalTransform of RootjointNode is already baked into the Skeletal and animation.
			//due to that we acquire the Parent SceneNode and get its GlobalTransform:
			if (!SceneNode->GetParentUid().IsEmpty())
			{
				if (const UInterchangeSceneNode* ParentSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid())))
				{
					ParentSceneNode->GetCustomGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalTransform);
				}
			}
		}
		ActorFactoryNode->SetCustomGlobalTransform(GlobalTransform);
	}

	ActorFactoryNode->SetCustomMobility(EComponentMobility::Static);

	if (TranslatedAssetNode)
	{
		SetUpFactoryNode(ActorFactoryNode, SceneNode, TranslatedAssetNode);
	}
}

UInterchangeActorFactoryNode* UInterchangeGenericLevelPipeline::CreateActorFactoryNode(const UInterchangeSceneNode* SceneNode, const UInterchangeBaseNode* TranslatedAssetNode) const
{
	if (!ensure(BaseNodeContainer))
	{
		return nullptr;
	}

	if(TranslatedAssetNode)
	{
		if(TranslatedAssetNode->IsA<UInterchangeCameraNode>())
		{
			return NewObject<UInterchangeCineCameraFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangeMeshNode>())
		{
			return NewObject<UInterchangeMeshActorFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangeSpotLightNode>())
		{
			return NewObject<UInterchangeSpotLightFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangePointLightNode>())
		{
			return NewObject<UInterchangePointLightFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangeRectLightNode>())
		{
			return NewObject<UInterchangeRectLightFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangeDirectionalLightNode>())
		{
			return NewObject<UInterchangeDirectionalLightFactoryNode>(BaseNodeContainer, NAME_None);
		}
	}

	return NewObject<UInterchangeActorFactoryNode>(BaseNodeContainer, NAME_None);
}

void UInterchangeGenericLevelPipeline::SetUpFactoryNode(UInterchangeActorFactoryNode* ActorFactoryNode, const UInterchangeSceneNode* SceneNode, const UInterchangeBaseNode* TranslatedAssetNode) const
{
	if (!ensure(BaseNodeContainer && ActorFactoryNode && SceneNode && TranslatedAssetNode))
	{
		return;
	}

	if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(TranslatedAssetNode))
	{
		if (MeshNode->IsSkinnedMesh())
		{
			bool bRootJointNode = SceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
			if (!bRootJointNode)
			{
				return;
			}

			ActorFactoryNode->SetCustomActorClassName(ASkeletalMeshActor::StaticClass()->GetPathName());
			ActorFactoryNode->SetCustomMobility(EComponentMobility::Movable);
		}
		else
		{
			ActorFactoryNode->SetCustomActorClassName(AStaticMeshActor::StaticClass()->GetPathName());
		}

		if (UInterchangeMeshActorFactoryNode* MeshActorFactoryNode = Cast<UInterchangeMeshActorFactoryNode>(ActorFactoryNode))
		{
			TMap<FString, FString> SlotMaterialDependencies;
			SceneNode->GetSlotMaterialDependencies(SlotMaterialDependencies);

			UE::Interchange::MeshesUtilities::ApplySlotMaterialDependencies(*MeshActorFactoryNode, SlotMaterialDependencies, *BaseNodeContainer);

			MeshActorFactoryNode->AddFactoryDependencyUid(UInterchangeFactoryBaseNode::BuildFactoryNodeUid(MeshNode->GetUniqueID()));
		}
	}
	else if (const UInterchangeBaseLightNode* BaseLightNode = Cast<UInterchangeBaseLightNode>(TranslatedAssetNode))
	{
		if (UInterchangeBaseLightFactoryNode* BaseLightFactoryNode = Cast<UInterchangeBaseLightFactoryNode>(ActorFactoryNode))
		{
			if (FLinearColor LightColor; BaseLightNode->GetCustomLightColor(LightColor))
			{
				BaseLightFactoryNode->SetCustomLightColor(LightColor.ToFColor(true));
			}

			if (float Intensity; BaseLightNode->GetCustomIntensity(Intensity))
			{
				BaseLightFactoryNode->SetCustomIntensity(Intensity);
			}

			if(bool bUseTemperature; BaseLightNode->GetCustomUseTemperature(bUseTemperature))
			{
				BaseLightFactoryNode->SetCustomUseTemperature(bUseTemperature);

				if(float Temperature; BaseLightNode->GetCustomTemperature(Temperature))
				{
					BaseLightFactoryNode->SetCustomTemperature(Temperature);
				}
			}

			using FLightUnits = std::underlying_type_t<ELightUnits>;
			using FInterchangeLightUnits = std::underlying_type_t<EInterchangeLightUnits>;
			using FCommonLightUnits = std::common_type_t<FLightUnits, FInterchangeLightUnits>;

			static_assert(FCommonLightUnits(EInterchangeLightUnits::Unitless) == FCommonLightUnits(ELightUnits::Unitless), "EInterchangeLightUnits::Unitless differs from ELightUnits::Unitless");
			static_assert(FCommonLightUnits(EInterchangeLightUnits::Lumens) == FCommonLightUnits(ELightUnits::Lumens), "EInterchangeLightUnits::Lumens differs from ELightUnits::Lumens");
			static_assert(FCommonLightUnits(EInterchangeLightUnits::Candelas) == FCommonLightUnits(ELightUnits::Candelas), "EInterchangeLightUnits::Candelas differs from ELightUnits::Candelas");

			if (const UInterchangeLightNode* LightNode = Cast<UInterchangeLightNode>(BaseLightNode))
			{
				if (UInterchangeLightFactoryNode* LightFactoryNode = Cast<UInterchangeLightFactoryNode>(BaseLightFactoryNode))
				{
					if (EInterchangeLightUnits IntensityUnits; LightNode->GetCustomIntensityUnits(IntensityUnits))
					{
						LightFactoryNode->SetCustomIntensityUnits(ELightUnits(IntensityUnits));
					}

					if (float AttenuationRadius; LightNode->GetCustomAttenuationRadius(AttenuationRadius))
					{
						LightFactoryNode->SetCustomAttenuationRadius(AttenuationRadius);
					}

					if(FString IESTexture; LightNode->GetCustomIESTexture(IESTexture))
					{
						LightFactoryNode->SetCustomIESTexture(IESTexture);
					}

					// RectLight
					if(const UInterchangeRectLightNode* RectLightNode = Cast<UInterchangeRectLightNode>(LightNode))
					{
						if(UInterchangeRectLightFactoryNode* RectLightFactoryNode = Cast<UInterchangeRectLightFactoryNode>(LightFactoryNode))
						{
							if(float SourceWidth; RectLightNode->GetCustomSourceWidth(SourceWidth))
							{
								RectLightFactoryNode->SetCustomSourceWidth(SourceWidth);
							}

							if(float SourceHeight; RectLightNode->GetCustomSourceHeight(SourceHeight))
							{
								RectLightFactoryNode->SetCustomSourceHeight(SourceHeight);
							}
						}
					}

					// Point Light
					if (const UInterchangePointLightNode* PointLightNode = Cast<UInterchangePointLightNode>(LightNode))
					{
						if (UInterchangePointLightFactoryNode* PointLightFactoryNode = Cast<UInterchangePointLightFactoryNode>(LightFactoryNode))
						{
							if (bool bUseInverseSquaredFalloff; PointLightNode->GetCustomUseInverseSquaredFalloff(bUseInverseSquaredFalloff))
							{
								PointLightFactoryNode->SetCustomUseInverseSquaredFalloff(bUseInverseSquaredFalloff);

								if (float LightFalloffExponent; PointLightNode->GetCustomLightFalloffExponent(LightFalloffExponent))
								{
									PointLightFactoryNode->SetCustomLightFalloffExponent(LightFalloffExponent);
								}
							}


							// Spot Light
							if (const UInterchangeSpotLightNode* SpotLightNode = Cast<UInterchangeSpotLightNode>(PointLightNode))
							{
								UInterchangeSpotLightFactoryNode* SpotLightFactoryNode = Cast<UInterchangeSpotLightFactoryNode>(PointLightFactoryNode);
								if (float InnerConeAngle; SpotLightNode->GetCustomInnerConeAngle(InnerConeAngle))
								{
									SpotLightFactoryNode->SetCustomInnerConeAngle(InnerConeAngle);
								}

								if (float OuterConeAngle; SpotLightNode->GetCustomOuterConeAngle(OuterConeAngle))
								{
									SpotLightFactoryNode->SetCustomOuterConeAngle(OuterConeAngle);
								}
							}
						}
					}
				}
			}
		}

		//Test for spot before point since a spot light is a point light
		if (BaseLightNode->IsA<UInterchangeSpotLightNode>())
		{
			ActorFactoryNode->SetCustomActorClassName(ASpotLight::StaticClass()->GetPathName());
		}
		else if (BaseLightNode->IsA<UInterchangePointLightNode>())
		{
			ActorFactoryNode->SetCustomActorClassName(APointLight::StaticClass()->GetPathName());
		}
		else if (BaseLightNode->IsA<UInterchangeRectLightNode>())
		{
			ActorFactoryNode->SetCustomActorClassName(ARectLight::StaticClass()->GetPathName());
		}
		else if (BaseLightNode->IsA<UInterchangeDirectionalLightNode>())
		{
			ActorFactoryNode->SetCustomActorClassName(ADirectionalLight::StaticClass()->GetPathName());
		}
		else
		{
			ActorFactoryNode->SetCustomActorClassName(APointLight::StaticClass()->GetPathName());
		}
	}
	else if (const UInterchangeCameraNode* CameraNode = Cast<UInterchangeCameraNode>(TranslatedAssetNode))
	{
		ActorFactoryNode->SetCustomActorClassName(ACineCameraActor::StaticClass()->GetPathName());
		ActorFactoryNode->SetCustomMobility(EComponentMobility::Movable);

		if (UInterchangeCineCameraFactoryNode* CineCameraFactoryNode = Cast<UInterchangeCineCameraFactoryNode>(ActorFactoryNode))
		{
			float FocalLength;
			if (CameraNode->GetCustomFocalLength(FocalLength))
			{
				CineCameraFactoryNode->SetCustomFocalLength(FocalLength);
			}

			float SensorHeight;
			if (CameraNode->GetCustomSensorHeight(SensorHeight))
			{
				CineCameraFactoryNode->SetCustomSensorHeight(SensorHeight);
			}

			float SensorWidth;
			if (CameraNode->GetCustomSensorWidth(SensorWidth))
			{
				CineCameraFactoryNode->SetCustomSensorWidth(SensorWidth);
			}

			bool bEnableDepthOfField;
			if (CameraNode->GetCustomEnableDepthOfField(bEnableDepthOfField))
			{
				CineCameraFactoryNode->SetCustomFocusMethod(bEnableDepthOfField ? ECameraFocusMethod::Manual : ECameraFocusMethod::DoNotOverride);
			}
			
		}
	}
}

void UInterchangeGenericLevelPipeline::ExecuteSceneVariantSetNodePreImport(const UInterchangeSceneVariantSetsNode& SceneVariantSetNode)
{
	if (!ensure(BaseNodeContainer))
	{
		return;
	}

	// We may eventually want to optionally import variants
	static bool bEnableSceneVariantSet = true;

	const FString FactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(SceneVariantSetNode.GetUniqueID());

	UInterchangeSceneVariantSetsFactoryNode* FactoryNode = NewObject<UInterchangeSceneVariantSetsFactoryNode>(BaseNodeContainer, NAME_None);

	FactoryNode->InitializeNode(FactoryNodeUid, SceneVariantSetNode.GetDisplayLabel(), EInterchangeNodeContainerType::FactoryData);
	FactoryNode->SetEnabled(bEnableSceneVariantSet);

	TArray<FString> VariantSetUids;
	SceneVariantSetNode.GetCustomVariantSetUids(VariantSetUids);

	for (const FString& VariantSetUid : VariantSetUids)
	{
		FactoryNode->AddCustomVariantSetUid(VariantSetUid);

		// Update factory's dependencies
		if (const UInterchangeVariantSetNode* TrackNode = Cast<UInterchangeVariantSetNode>(BaseNodeContainer->GetNode(VariantSetUid)))
		{
			TArray<FString> DependencyNodeUids;
			TrackNode->GetCustomDependencyUids(DependencyNodeUids);

			for (const FString& DependencyNodeUid : DependencyNodeUids)
			{
				
				const FString DependencyFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(DependencyNodeUid);
				FactoryNode->AddFactoryDependencyUid(DependencyFactoryNodeUid);

				if (UInterchangeFactoryBaseNode* DependencyFactoryNode = BaseNodeContainer->GetFactoryNode(DependencyFactoryNodeUid))
				{
					if (bEnableSceneVariantSet && !DependencyFactoryNode->IsEnabled())
					{
						DependencyFactoryNode->SetEnabled(true);
					}
				}
			}
		}
	}

	UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(&SceneVariantSetNode, FactoryNode, false);

	FactoryNode->AddTargetNodeUid(SceneVariantSetNode.GetUniqueID());
	SceneVariantSetNode.AddTargetNodeUid(FactoryNode->GetUniqueID());

	BaseNodeContainer->AddNode(FactoryNode);
}
