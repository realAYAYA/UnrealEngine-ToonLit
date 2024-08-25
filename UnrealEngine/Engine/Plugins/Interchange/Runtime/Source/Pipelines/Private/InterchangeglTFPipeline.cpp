// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeglTFPipeline.h"

#include "InterchangePipelineLog.h"

#include "InterchangeMeshFactoryNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeMaterialInstanceNode.h"
#include "InterchangeMaterialFactoryNode.h"

#include "Gltf/InterchangeGLTFMaterial.h"

#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Misc/App.h"

const TArray<FString> UGLTFPipelineSettings::ExpectedMaterialInstanceIdentifiers = {TEXT("MI_Default_Opaque"), TEXT("MI_Default_Mask"), TEXT("MI_Default_Blend"), 
																					TEXT("MI_Unlit_Opaque"), TEXT("MI_Unlit_Mask"), TEXT("MI_Unlit_Blend"), 
																					TEXT("MI_ClearCoat_Opaque"), TEXT("MI_ClearCoat_Mask"), TEXT("MI_ClearCoat_Blend"),
																					TEXT("MI_Sheen_Opaque"), TEXT("MI_Sheen_Mask"), TEXT("MI_Sheen_Blend"), 
																					TEXT("MI_Transmission"), 
																					TEXT("MI_SpecularGlossiness_Opaque"), TEXT("MI_SpecularGlossiness_Mask"), TEXT("MI_SpecularGlossiness_Blend"), 
																					TEXT("MI_Default_Opaque_DS"), TEXT("MI_Default_Mask_DS"), TEXT("MI_Default_Blend_DS"), 
																					TEXT("MI_Unlit_Opaque_DS"), TEXT("MI_Unlit_Mask_DS"), TEXT("MI_Unlit_Blend_DS"), 
																					TEXT("MI_ClearCoat_Opaque_DS"), TEXT("MI_ClearCoat_Mask_DS"), TEXT("MI_ClearCoat_Blend_DS"), 
																					TEXT("MI_Sheen_Opaque_DS"), TEXT("MI_Sheen_Mask_DS"), TEXT("MI_Sheen_Blend_DS"), 
																					TEXT("MI_Transmission_DS"), 
																					TEXT("MI_SpecularGlossiness_Opaque_DS"), TEXT("MI_SpecularGlossiness_Mask_DS"), TEXT("MI_SpecularGlossiness_Blend_DS")};

TArray<FString> UGLTFPipelineSettings::ValidateMaterialInstancesAndParameters() const
{
	TArray<FString> NotCoveredIdentifiersParameters;

	//Check if all Material variations are covered:
	TArray<FString> ExpectedIdentifiers = ExpectedMaterialInstanceIdentifiers;
	TArray<FString> IdentifiersUsed;
	MaterialParents.GetKeys(IdentifiersUsed);
	for (const FString& Identifier : IdentifiersUsed)
	{
		ExpectedIdentifiers.Remove(Identifier);
	}
	for (const FString& ExpectedIdentifier : ExpectedIdentifiers)
	{
		NotCoveredIdentifiersParameters.Add(TEXT("[") + ExpectedIdentifier + TEXT("]: MaterialInstance not found for Identifier."));
	}

	for (const TPair<FString, FSoftObjectPath>& MaterialParent : MaterialParents)
	{
		TSet<FString> ExpectedParameters = GenerateExpectedParametersList(MaterialParent.Key);

		if (UMaterialInstance* ParentMaterialInstance = Cast<UMaterialInstance>(MaterialParent.Value.TryLoad()))
		{
			TArray<FGuid> ParameterIds;
			TArray<FMaterialParameterInfo> ScalarParameterInfos;
			TArray<FMaterialParameterInfo> VectorParameterInfos;
			TArray<FMaterialParameterInfo> TextureParameterInfos;
			ParentMaterialInstance->GetAllScalarParameterInfo(ScalarParameterInfos, ParameterIds);
			ParentMaterialInstance->GetAllVectorParameterInfo(VectorParameterInfos, ParameterIds);
			ParentMaterialInstance->GetAllTextureParameterInfo(TextureParameterInfos, ParameterIds);

			for (const FMaterialParameterInfo& ParameterInfo : ScalarParameterInfos)
			{
				ExpectedParameters.Remove(ParameterInfo.Name.ToString());
			}
			for (const FMaterialParameterInfo& ParameterInfo : VectorParameterInfos)
			{
				ExpectedParameters.Remove(ParameterInfo.Name.ToString());
			}
			for (const FMaterialParameterInfo& ParameterInfo : TextureParameterInfos)
			{
				ExpectedParameters.Remove(ParameterInfo.Name.ToString());
			}
		}

		for (const FString& ExpectedParameter : ExpectedParameters)
		{
			NotCoveredIdentifiersParameters.Add(TEXT("[") + MaterialParent.Key + TEXT("]: Does not cover expected parameter: ") + ExpectedParameter + TEXT("."));
		}
	}

	return NotCoveredIdentifiersParameters;
}

TSet<FString> UGLTFPipelineSettings::GenerateExpectedParametersList(const FString& Identifier) const
{
	using namespace UE::Interchange::GLTFMaterials;

	TSet<FString> ExpectedParameters;

	auto AddTextureAndRelated = [&ExpectedParameters](const FString& TextureName)
		{
			ExpectedParameters.Add(TextureName);
			ExpectedParameters.Add(TextureName + Inputs::PostFix::OffsetScale);
			ExpectedParameters.Add(TextureName + Inputs::PostFix::Rotation);
			ExpectedParameters.Add(TextureName + Inputs::PostFix::TexCoord);
			ExpectedParameters.Add(TextureName + Inputs::PostFix::TilingMethod);
		};

	if (Identifier.Contains(TEXT("_Unlit")))
	{
		AddTextureAndRelated(Inputs::BaseColorTexture);
		ExpectedParameters.Add(Inputs::BaseColorFactor);

		return ExpectedParameters;
	}

	//Generic ones:
	{
		AddTextureAndRelated(Inputs::NormalTexture);
		ExpectedParameters.Add(Inputs::NormalScale);

		if (!Identifier.Contains(TEXT("Transmission")))
		{
			AddTextureAndRelated(Inputs::EmissiveTexture);
			ExpectedParameters.Add(Inputs::EmissiveFactor);
			ExpectedParameters.Add(Inputs::EmissiveStrength);
		}

		AddTextureAndRelated(Inputs::OcclusionTexture);
		ExpectedParameters.Add(Inputs::OcclusionStrength);

		if (!Identifier.Contains(TEXT("SpecularGlossiness")))
		{
			ExpectedParameters.Add(Inputs::IOR);

			AddTextureAndRelated(Inputs::SpecularTexture);
			ExpectedParameters.Add(Inputs::SpecularFactor);
		}
	}

	//Based on ShadingModel:

	if (Identifier.Contains(TEXT("Default")))
	{
		//MetalRoughness Specific:

		AddTextureAndRelated(Inputs::BaseColorTexture);
		ExpectedParameters.Add(Inputs::BaseColorFactor);

		AddTextureAndRelated(Inputs::MetallicRoughnessTexture);
		ExpectedParameters.Add(Inputs::MetallicFactor);
		ExpectedParameters.Add(Inputs::RoughnessFactor);
	}
	else if (Identifier.Contains(TEXT("ClearCoat")))
	{
		AddTextureAndRelated(Inputs::ClearCoatTexture);
		ExpectedParameters.Add(Inputs::ClearCoatFactor);

		AddTextureAndRelated(Inputs::ClearCoatRoughnessTexture);
		ExpectedParameters.Add(Inputs::ClearCoatRoughnessFactor);

		AddTextureAndRelated(Inputs::ClearCoatNormalTexture);
		ExpectedParameters.Add(Inputs::ClearCoatNormalScale);
	}
	else if (Identifier.Contains(TEXT("Sheen")))
	{
		AddTextureAndRelated(Inputs::SheenColorTexture);
		ExpectedParameters.Add(Inputs::SheenColorFactor);

		AddTextureAndRelated(Inputs::SheenRoughnessTexture);
		ExpectedParameters.Add(Inputs::SheenRoughnessFactor);
	}
	else if (Identifier.Contains(TEXT("Transmission")))
	{
		AddTextureAndRelated(Inputs::TransmissionTexture);
		ExpectedParameters.Add(Inputs::TransmissionFactor);
	}
	else if (Identifier.Contains(TEXT("SpecularGlossiness")))
	{
		AddTextureAndRelated(Inputs::DiffuseTexture);
		ExpectedParameters.Add(Inputs::DiffuseFactor);

		AddTextureAndRelated(Inputs::SpecularGlossinessTexture);
		ExpectedParameters.Add(Inputs::SpecFactor);
		ExpectedParameters.Add(Inputs::GlossinessFactor);
	}

	return ExpectedParameters;
}

void UGLTFPipelineSettings::BuildMaterialInstance(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode)
{
	using namespace UE::Interchange::GLTFMaterials;

	TArray<UE::Interchange::FAttributeKey> AttributeKeys;
	ShaderGraphNode->GetAttributeKeys(AttributeKeys);

	TMap<FString, UE::Interchange::FAttributeKey> GltfAttributeKeys;
	for (const UE::Interchange::FAttributeKey& AttributeKey : AttributeKeys)
	{
		if (AttributeKey.ToString().Contains(InterchangeGltfMaterialAttributeIdentifier))
		{
			GltfAttributeKeys.Add(AttributeKey.ToString().Replace(*InterchangeGltfMaterialAttributeIdentifier, TEXT(""), ESearchCase::CaseSensitive), AttributeKey);
		}
	}
	if (GltfAttributeKeys.Num() == 0)
	{
		return;
	}

	FString ParentIdentifier;
	if (!ShaderGraphNode->GetStringAttribute(*(InterchangeGltfMaterialAttributeIdentifier + TEXT("ParentIdentifier")), ParentIdentifier))
	{
		return;
	}
	GltfAttributeKeys.Remove(TEXT("ParentIdentifier"));

	FString Parent;
	if (const FSoftObjectPath* ObjectPath = MaterialParents.Find(ParentIdentifier))
	{
		Parent = ObjectPath->GetAssetPathString();
	}
	else
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("[Interchange] Failed to load MaterialParent for ParentIdentifier: %s"), *ParentIdentifier);
		return;
	}

	FString MaterialFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(ShaderGraphNode->GetUniqueID());
	FString MaterialFactoryNodeName = ShaderGraphNode->GetDisplayLabel();

	MaterialInstanceFactoryNode->InitializeNode(MaterialFactoryNodeUid, MaterialFactoryNodeName, EInterchangeNodeContainerType::FactoryData);

	MaterialInstanceFactoryNode->SetCustomParent(Parent);

	const UClass* MaterialClass = FApp::IsGame() ? UMaterialInstanceDynamic::StaticClass() : UMaterialInstanceConstant::StaticClass();
	MaterialInstanceFactoryNode->SetCustomInstanceClassName(MaterialClass->GetPathName());

	for (const TPair<FString, UE::Interchange::FAttributeKey>& GltfAttributeKey : GltfAttributeKeys)
	{
		UE::Interchange::EAttributeTypes AttributeType = ShaderGraphNode->GetAttributeType(GltfAttributeKey.Value);

		FString InputValueKey = UInterchangeShaderPortsAPI::MakeInputValueKey(GltfAttributeKey.Key);

		//we are only using 3 attribute types:
		switch (AttributeType)
		{
		case UE::Interchange::EAttributeTypes::Float:
		{
			float Value;
			if (ShaderGraphNode->GetFloatAttribute(GltfAttributeKey.Value.Key, Value))
			{
				MaterialInstanceFactoryNode->AddFloatAttribute(InputValueKey, Value);
			}
		}
		break;
		case UE::Interchange::EAttributeTypes::LinearColor:
		{
			FLinearColor Value;
			if (ShaderGraphNode->GetLinearColorAttribute(GltfAttributeKey.Value.Key, Value))
			{
				MaterialInstanceFactoryNode->AddLinearColorAttribute(InputValueKey, Value);
			}
		}
		break;
		case UE::Interchange::EAttributeTypes::String:
		{
			FString TextureUid;
			if (ShaderGraphNode->GetStringAttribute(GltfAttributeKey.Value.Key, TextureUid))
			{
				FString FactoryTextureUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(TextureUid);

				MaterialInstanceFactoryNode->AddStringAttribute(InputValueKey, FactoryTextureUid);
				MaterialInstanceFactoryNode->AddFactoryDependencyUid(FactoryTextureUid);
			}
		}
		break;
		default:
			break;
		}
	}
}

UInterchangeGLTFPipeline::UInterchangeGLTFPipeline()
	: GLTFPipelineSettings(UGLTFPipelineSettings::StaticClass()->GetDefaultObject<UGLTFPipelineSettings>())
{
}

void UInterchangeGLTFPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

	TArray<FString> MaterialInstanceIssues = GLTFPipelineSettings->ValidateMaterialInstancesAndParameters();
	for (const FString& MaterialInstanceIssue : MaterialInstanceIssues)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("%s"), *MaterialInstanceIssue);
	}
}

void UInterchangeGLTFPipeline::ExecutePipeline(UInterchangeBaseNodeContainer* NodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas, const FString& ContentBasePath)
{
	Super::ExecutePipeline(NodeContainer, InSourceDatas, ContentBasePath);

	if (GLTFPipelineSettings)
	{
		TMap<FString, const UInterchangeShaderGraphNode*> MaterialFactoryNodeUidsToShaderGraphNodes;
		auto FindGLTFShaderGraphNode = [&MaterialFactoryNodeUidsToShaderGraphNodes, &NodeContainer](const FString& NodeUid, UInterchangeFactoryBaseNode* /*Material or MaterialInstance*/ FactoryNode)
		{
			TArray<FString> TargetNodeUids;
			FactoryNode->GetTargetNodeUids(TargetNodeUids);

			for (const FString& TargetNodeUid : TargetNodeUids)
			{

				if (const UInterchangeShaderGraphNode* ShaderGraphNode = Cast<UInterchangeShaderGraphNode>(NodeContainer->GetNode(TargetNodeUid)))
				{
					FString ParentIdentifier;
					if (ShaderGraphNode->GetStringAttribute(*(InterchangeGltfMaterialAttributeIdentifier + TEXT("ParentIdentifier")), ParentIdentifier))
					{
						MaterialFactoryNodeUidsToShaderGraphNodes.Add(NodeUid, ShaderGraphNode);
						break;
					}
				}
			}
		};
		NodeContainer->IterateNodesOfType<UInterchangeMaterialFactoryNode>([&MaterialFactoryNodeUidsToShaderGraphNodes, &NodeContainer, &FindGLTFShaderGraphNode](const FString& NodeUid, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
			{
				FindGLTFShaderGraphNode(NodeUid, MaterialFactoryNode);
			});

		NodeContainer->IterateNodesOfType<UInterchangeMaterialInstanceFactoryNode>([&MaterialFactoryNodeUidsToShaderGraphNodes, &NodeContainer, &FindGLTFShaderGraphNode](const FString& NodeUid, UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode)
			{
				FindGLTFShaderGraphNode(NodeUid, MaterialInstanceFactoryNode);
			});

		for (const TPair<FString, const UInterchangeShaderGraphNode*>& ShaderGraphNode : MaterialFactoryNodeUidsToShaderGraphNodes)
		{
			UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode = NewObject<UInterchangeMaterialInstanceFactoryNode>(NodeContainer);
			GLTFPipelineSettings->BuildMaterialInstance(ShaderGraphNode.Value, MaterialInstanceFactoryNode);

			NodeContainer->ReplaceNode(ShaderGraphNode.Key, MaterialInstanceFactoryNode);
		}
	}
}

