// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeGenericMaterialPipeline.h"

#include "CoreMinimal.h"

#include "InterchangeGenericTexturePipeline.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTexture2DArrayNode.h"
#include "InterchangeTextureCubeNode.h"
#include "InterchangeTextureFactoryNode.h"
#include "InterchangeTextureNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNode.h"

#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureSampleParameter2DArray.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CoreMisc.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeSourceNode.h"
#include "Templates/Function.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGenericMaterialPipeline)

#define LOCTEXT_NAMESPACE "InterchangeGenericMaterialPipeline"

FString LexToString(UInterchangeGenericMaterialPipeline::EMaterialInputType Value)
{
	switch(Value)
	{
	case UInterchangeGenericMaterialPipeline::EMaterialInputType::Unknown:
		return TEXT("Unknown");
	case UInterchangeGenericMaterialPipeline::EMaterialInputType::Color:
		return TEXT("Color");
	case UInterchangeGenericMaterialPipeline::EMaterialInputType::Vector:
		return TEXT("Vector");
	case UInterchangeGenericMaterialPipeline::EMaterialInputType::Scalar:
		return TEXT("Scalar");
	default:
		ensure(false);
		return FString();
	}
}

namespace UE::Interchange::InterchangeGenericMaterialPipeline::Private
{
	bool AreRequiredPackagesLoaded()
	{
		auto ArePackagesLoaded = [](const TArray<FString>& PackagePaths) -> bool
		{
			bool bAllLoaded = true;

			for (const FString& PackagePath : PackagePaths)
			{
				const FString ObjectPath(FPackageName::ExportTextPathToObjectPath(PackagePath));

				if (FPackageName::DoesPackageExist(ObjectPath))
				{
					if (FSoftObjectPath(ObjectPath).TryLoad())
					{
						continue;
					}
					else
					{
						UE_LOG(LogInterchangePipeline, Warning, TEXT("Couldn't load %s"), *PackagePath);
					}
				}
				else
				{
					UE_LOG(LogInterchangePipeline, Warning, TEXT("Couldn't find %s"), *PackagePath);
				}

				bAllLoaded = false;
			}

			return bAllLoaded;
		};

		TArray<FString> RequiredPackages = {
			TEXT("MaterialFunction'/Interchange/Functions/MX_StandardSurface.MX_StandardSurface'"),
			TEXT("MaterialFunction'/Interchange/Functions/MX_TransmissionSurface.MX_TransmissionSurface'"),
			TEXT("MaterialFunction'/Engine/Functions/Engine_MaterialFunctions01/Shading/ConvertFromDiffSpec.ConvertFromDiffSpec'"),
			TEXT("MaterialFunction'/Engine/Functions/Engine_MaterialFunctions01/Texturing/FlattenNormal.FlattenNormal'"),
			TEXT("MaterialFunction'/Engine/Functions/Engine_MaterialFunctions02/Utility/MakeFloat3.MakeFloat3'"),
			TEXT("MaterialFunction'/Engine/Functions/Engine_MaterialFunctions02/Texturing/CustomRotator.CustomRotator'"),
		};

		static const bool bRequiredPackagesLoaded = ArePackagesLoaded(RequiredPackages);

		return bRequiredPackagesLoaded;
	}

	void UpdateBlendModeBasedOnOpacityAttributes(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
	{
		// Opacity Clip Value
		bool bIsMasked = false;
		{
			float OpacityClipValue;
			if (ShaderGraphNode->GetCustomOpacityMaskClipValue(OpacityClipValue))
			{
				MaterialFactoryNode->SetCustomOpacityMaskClipValue(OpacityClipValue);
				bIsMasked = true;
			}
		}

		// Don't change the blend mode if it was already set
		TEnumAsByte<EBlendMode> BlendMode = bIsMasked ? EBlendMode::BLEND_Masked : EBlendMode::BLEND_Translucent;
		if (!MaterialFactoryNode->GetCustomBlendMode(BlendMode))
		{
			MaterialFactoryNode->SetCustomBlendMode(BlendMode);
		}

		// If bland mode is masked or translucent, set lighting mode accordingly without changing it if it was already set
		if(BlendMode == EBlendMode::BLEND_Masked || BlendMode == EBlendMode::BLEND_Translucent)
		{
			TEnumAsByte<ETranslucencyLightingMode> LightingMode = ETranslucencyLightingMode::TLM_Surface;
			if (!MaterialFactoryNode->GetCustomTranslucencyLightingMode(LightingMode))
			{
				MaterialFactoryNode->SetCustomTranslucencyLightingMode(LightingMode);
			}
		}
	}
}

UInterchangeGenericMaterialPipeline::UInterchangeGenericMaterialPipeline()
{
	TexturePipeline = CreateDefaultSubobject<UInterchangeGenericTexturePipeline>("TexturePipeline");
}

void UInterchangeGenericMaterialPipeline::PreDialogCleanup(const FName PipelineStackName)
{
	if (TexturePipeline)
	{
		TexturePipeline->PreDialogCleanup(PipelineStackName);
	}

	SaveSettings(PipelineStackName);
}

bool UInterchangeGenericMaterialPipeline::IsSettingsAreValid(TOptional<FText>& OutInvalidReason) const
{
	if (TexturePipeline && !TexturePipeline->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	return Super::IsSettingsAreValid(OutInvalidReason);
}

void UInterchangeGenericMaterialPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

	if (TexturePipeline)
	{
		TexturePipeline->AdjustSettingsForContext(ImportType, ReimportAsset);
	}

	TArray<FString> HideCategories;
	bool bIsObjectAMaterial = !ReimportAsset ? false : ReimportAsset->IsA(UMaterialInterface::StaticClass());
	if ((!bIsObjectAMaterial && ImportType == EInterchangePipelineContext::AssetReimport)
		|| ImportType == EInterchangePipelineContext::AssetCustomLODImport
		|| ImportType == EInterchangePipelineContext::AssetCustomLODReimport
		|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningImport
		|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningReimport)
	{
		bImportMaterials = false;
		HideCategories.Add(TEXT("Materials"));
	}

	if (UInterchangePipelineBase* OuterMostPipeline = GetMostPipelineOuter())
	{
		for (const FString& HideCategoryName : HideCategories)
		{
			HidePropertiesOfCategory(OuterMostPipeline, this, HideCategoryName);
		}
	}

	using namespace UE::Interchange;

	if (!InterchangeGenericMaterialPipeline::Private::AreRequiredPackagesLoaded())
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericMaterialPipeline: Some required packages are missing. Material import might be wrong"));
	}
}

void UInterchangeGenericMaterialPipeline::ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas)
{
	if (!InBaseNodeContainer)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericMaterialPipeline: Cannot execute pre-import pipeline because InBaseNodeContrainer is null"));
		return;
	}

	//Set the result container to allow error message
	//The parent Results container should be set at this point
	ensure(Results);
	{
		if (TexturePipeline)
		{
			TexturePipeline->SetResultsContainer(Results);
		}
	}

	BaseNodeContainer = InBaseNodeContainer;
	SourceDatas.Empty(InSourceDatas.Num());
	for (const UInterchangeSourceData* SourceData : InSourceDatas)
	{
		SourceDatas.Add(SourceData);
	}
	
	if (TexturePipeline)
	{
		TexturePipeline->ScriptedExecutePreImportPipeline(InBaseNodeContainer, InSourceDatas);
	}

	//Skip Material import if the toggle is off
	if (!bImportMaterials)
	{
		return;
	}

	//Find all translated node we need for this pipeline
	BaseNodeContainer->IterateNodes([this](const FString& NodeUid, UInterchangeBaseNode* Node)
	{
		switch(Node->GetNodeContainerType())
		{
			case EInterchangeNodeContainerType::TranslatedAsset:
			{
				if (UInterchangeShaderGraphNode* MaterialNode = Cast<UInterchangeShaderGraphNode>(Node))
				{
					MaterialNodes.Add(MaterialNode);
				}
			}
			break;
		}
	});

	// Check to see whether materials should be created even if unused
	// By default we do not create the materials, every node with mesh attribute can enable them. So we won't create unused materials.
	bool bImportUnusedMaterial = false;
	if (const UInterchangeSourceNode* SourceNode = UInterchangeSourceNode::GetUniqueInstance(BaseNodeContainer))
	{
		SourceNode->GetCustomImportUnusedMaterial(bImportUnusedMaterial);
	}

#if !WITH_EDITOR
	// Can't import materials at runtime, fallback to instances
	if (MaterialImport == EInterchangeMaterialImportOption::ImportAsMaterials)
	{
		MaterialImport = EInterchangeMaterialImportOption::ImportAsMaterialInstances;
	}
#endif // !WITH_EDITOR

	if (MaterialImport == EInterchangeMaterialImportOption::ImportAsMaterials)
	{
		for (const UInterchangeShaderGraphNode* ShaderGraphNode : MaterialNodes)
		{
			UInterchangeBaseMaterialFactoryNode* MaterialBaseFactoryNode = nullptr;

			bool bIsAShaderFunction;
			if (ShaderGraphNode->GetCustomIsAShaderFunction(bIsAShaderFunction) && bIsAShaderFunction)
			{
				MaterialBaseFactoryNode = CreateMaterialFunctionFactoryNode(ShaderGraphNode);
			}
			else
			{
				MaterialBaseFactoryNode = CreateMaterialFactoryNode(ShaderGraphNode);
			}

			if (MaterialBaseFactoryNode)
			{
				MaterialBaseFactoryNode->SetEnabled(bImportUnusedMaterial);
			}
		}
	}
	else if (MaterialImport == EInterchangeMaterialImportOption::ImportAsMaterialInstances)
	{
		for (const UInterchangeShaderGraphNode* ShaderGraphNode : MaterialNodes)
		{
			if (UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode = CreateMaterialInstanceFactoryNode(ShaderGraphNode))
			{
				MaterialInstanceFactoryNode->SetEnabled(bImportUnusedMaterial);
			}
		}
	}
}

void UInterchangeGenericMaterialPipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	if (TexturePipeline)
	{
		TexturePipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}
}

void UInterchangeGenericMaterialPipeline::SetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex)
{
	if (TexturePipeline)
	{
		TexturePipeline->ScriptedSetReimportSourceIndex(ReimportObjectClass, SourceFileIndex);
	}
}

UInterchangeBaseMaterialFactoryNode* UInterchangeGenericMaterialPipeline::CreateBaseMaterialFactoryNode(const UInterchangeBaseNode* MaterialNode, TSubclassOf<UInterchangeBaseMaterialFactoryNode> NodeType)
{
	FString DisplayLabel = MaterialNode->GetDisplayLabel();
	const FString NodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(MaterialNode->GetUniqueID());
	UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode = nullptr;
	if (BaseNodeContainer->IsNodeUidValid(NodeUid))
	{
		//The node already exist, just return it
		MaterialFactoryNode = Cast<UInterchangeBaseMaterialFactoryNode>(BaseNodeContainer->GetFactoryNode(NodeUid));
		if (!ensure(MaterialFactoryNode))
		{
			//Log an error
		}
	}
	else
	{
		MaterialFactoryNode = NewObject<UInterchangeBaseMaterialFactoryNode>(BaseNodeContainer, NodeType.Get(), NAME_None);
		if (!ensure(MaterialFactoryNode))
		{
			return nullptr;
		}
		//Creating a Material
		MaterialFactoryNode->InitializeNode(NodeUid, DisplayLabel, EInterchangeNodeContainerType::FactoryData);
		
		BaseNodeContainer->AddNode(MaterialFactoryNode);
		MaterialFactoryNodes.Add(MaterialFactoryNode);
		MaterialFactoryNode->AddTargetNodeUid(MaterialNode->GetUniqueID());
		MaterialNode->AddTargetNodeUid(MaterialFactoryNode->GetUniqueID());
	}
	return MaterialFactoryNode;
}

bool UInterchangeGenericMaterialPipeline::IsClearCoatModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials::ClearCoat;

	const bool bHasClearCoatInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::ClearCoat);

	return bHasClearCoatInput;
}

bool UInterchangeGenericMaterialPipeline::IsSheenModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials::Sheen;

	const bool bHasSheenColorInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::SheenColor);

	return bHasSheenColorInput;
}

bool UInterchangeGenericMaterialPipeline::IsSubsurfaceModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials::Subsurface;

	const bool bHasSubsurfaceColorInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::SubsurfaceColor);

	return bHasSubsurfaceColorInput;
}

bool UInterchangeGenericMaterialPipeline::IsThinTranslucentModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials::ThinTranslucent;

	const bool bHasTransmissionColorInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::TransmissionColor);

	return bHasTransmissionColorInput;
}

bool UInterchangeGenericMaterialPipeline::IsPBRModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials::PBR;

	const bool bHasBaseColorInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::BaseColor);

	return bHasBaseColorInput;
}

bool UInterchangeGenericMaterialPipeline::IsPhongModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials::Phong;

	const bool bHasDiffuseInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::DiffuseColor);
	const bool bHasSpecularInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::SpecularColor);

	return bHasDiffuseInput && bHasSpecularInput;
}

bool UInterchangeGenericMaterialPipeline::IsLambertModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials::Lambert;

	const bool bHasDiffuseInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::DiffuseColor);

	return bHasDiffuseInput;
}

bool UInterchangeGenericMaterialPipeline::IsStandardSurfaceModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials;
	FString ShaderType;
	ShaderGraphNode->GetCustomShaderType(ShaderType);

	if(ShaderType == StandardSurface::Name.ToString())
	{
		return true;
	}

	return false;
}

bool UInterchangeGenericMaterialPipeline::HandlePhongModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials::Phong;

	if (IsPhongModel(ShaderGraphNode))
	{
		// ConvertFromDiffSpec function call
		UInterchangeMaterialExpressionFactoryNode* FunctionCallExpression = NewObject<UInterchangeMaterialExpressionFactoryNode>(BaseNodeContainer, NAME_None);
		FunctionCallExpression->SetCustomExpressionClassName(UMaterialExpressionMaterialFunctionCall::StaticClass()->GetName());
		FString FunctionCallExpressionUid = MaterialFactoryNode->GetUniqueID() + TEXT("\\Inputs\\BaseColor\\DiffSpecFunc");
		FunctionCallExpression->InitializeNode(FunctionCallExpressionUid, TEXT("DiffSpecFunc"), EInterchangeNodeContainerType::FactoryData);

		BaseNodeContainer->AddNode(FunctionCallExpression);
		BaseNodeContainer->SetNodeParentUid(FunctionCallExpressionUid, MaterialFactoryNode->GetUniqueID());

		const FName MaterialFunctionMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionMaterialFunctionCall, MaterialFunction);

		FunctionCallExpression->AddStringAttribute(
			MaterialFunctionMemberName,
			TEXT("MaterialFunction'/Engine/Functions/Engine_MaterialFunctions01/Shading/ConvertFromDiffSpec.ConvertFromDiffSpec'"));
		FunctionCallExpression->AddApplyAndFillDelegates<FString>(MaterialFunctionMemberName, UMaterialExpressionMaterialFunctionCall::StaticClass(), MaterialFunctionMemberName);

		MaterialFactoryNode->ConnectOutputToBaseColor(FunctionCallExpressionUid, UE::Interchange::Materials::PBR::Parameters::BaseColor.ToString());
		MaterialFactoryNode->ConnectOutputToMetallic(FunctionCallExpressionUid, UE::Interchange::Materials::PBR::Parameters::Metallic.ToString());
		MaterialFactoryNode->ConnectOutputToSpecular(FunctionCallExpressionUid, UE::Interchange::Materials::PBR::Parameters::Specular.ToString());
		
		// Diffuse
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> DiffuseExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::DiffuseColor.ToString(), FunctionCallExpression->GetUniqueID());

			if (DiffuseExpressionFactoryNode.Get<0>())
			{
				UInterchangeShaderPortsAPI::ConnectOuputToInput(FunctionCallExpression, Parameters::DiffuseColor.ToString(),
					DiffuseExpressionFactoryNode.Get<0>()->GetUniqueID(), DiffuseExpressionFactoryNode.Get<1>());
			}
		}

		// Specular Color
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> SpecularExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::SpecularColor.ToString(), FunctionCallExpression->GetUniqueID());

			if (SpecularExpressionFactoryNode.Get<0>())
			{
				UInterchangeShaderPortsAPI::ConnectOuputToInput(FunctionCallExpression, Parameters::SpecularColor.ToString(),
					SpecularExpressionFactoryNode.Get<0>()->GetUniqueID(), SpecularExpressionFactoryNode.Get<1>());
			}
		}
		
		// Shininess
		{
			const bool bHasShininessInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::Shininess);
			if (bHasShininessInput)
			{
				TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

				TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ShininessExpressionFactoryNode =
					CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::Shininess.ToString(), MaterialFactoryNode->GetUniqueID());

				if (ShininessExpressionFactoryNode.Get<0>())
				{
					UInterchangeMaterialExpressionFactoryNode* DivideShininessNode =
						CreateExpressionNode(TEXT("DivideShininess"), ShininessExpressionFactoryNode.Get<0>()->GetUniqueID(), UMaterialExpressionDivide::StaticClass());

					const float ShininessScale = 100.f; // Divide shininess by 100 to bring it into a 0-1 range for roughness.
					const FName ShininessScaleParameterName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionDivide, ConstB);
					DivideShininessNode->AddFloatAttribute(ShininessScaleParameterName, ShininessScale);
					DivideShininessNode->AddApplyAndFillDelegates<float>(ShininessScaleParameterName, UMaterialExpressionDivide::StaticClass(),  GET_MEMBER_NAME_CHECKED(UMaterialExpressionDivide, ConstB));

					// Connect Shininess to Divide
					UInterchangeShaderPortsAPI::ConnectOuputToInput(DivideShininessNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionDivide, A).ToString(),
						ShininessExpressionFactoryNode.Get<0>()->GetUniqueID(), ShininessExpressionFactoryNode.Get<1>());

					UInterchangeMaterialExpressionFactoryNode* InverseShininessNode =
						CreateExpressionNode(TEXT("InverseShininess"), ShininessExpressionFactoryNode.Get<0>()->GetUniqueID(), UMaterialExpressionOneMinus::StaticClass());

					// Connect Divide to Inverse
					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(InverseShininessNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionOneMinus, Input).ToString(),
						DivideShininessNode->GetUniqueID());

					MaterialFactoryNode->ConnectToRoughness(InverseShininessNode->GetUniqueID());
				}
			}
		}

		return true;
	}

	return false;
}

bool UInterchangeGenericMaterialPipeline::HandleLambertModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials::Lambert;

	if (IsLambertModel(ShaderGraphNode))
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> DiffuseExpressionFactoryNode =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::DiffuseColor.ToString(), MaterialFactoryNode->GetUniqueID());

		if (DiffuseExpressionFactoryNode.Get<0>())
		{
			MaterialFactoryNode->ConnectOutputToBaseColor(DiffuseExpressionFactoryNode.Get<0>()->GetUniqueID(), DiffuseExpressionFactoryNode.Get<1>());
		}

		return true;
	}

	return false;
}

bool UInterchangeGenericMaterialPipeline::HandlePBRModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials::PBR;

	bool bShadingModelHandled = false;

	// BaseColor
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::BaseColor);

		if (bHasInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::BaseColor.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToBaseColor(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	// Metallic
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::Metallic);

		if (bHasInput)
		{
			TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::Metallic.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToMetallic(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	// Specular
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::Specular);

		if (bHasInput)
		{
			TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::Specular.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToSpecular(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	// Roughness
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::Roughness);

		if (bHasInput)
		{
			TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::Roughness.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToRoughness(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	return bShadingModelHandled;
}

bool UInterchangeGenericMaterialPipeline::HandleStandardSurfaceModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	if(IsStandardSurfaceModel(ShaderGraphNode))
	{
		UInterchangeMaterialExpressionFactoryNode* FunctionCallExpression = NewObject<UInterchangeMaterialExpressionFactoryNode>(BaseNodeContainer, NAME_None);
		FunctionCallExpression->SetCustomExpressionClassName(UMaterialExpressionMaterialFunctionCall::StaticClass()->GetName());
		FString FunctionCallExpressionUid = MaterialFactoryNode->GetUniqueID() + TEXT("StandardSurface");
		FunctionCallExpression->InitializeNode(FunctionCallExpressionUid, TEXT("StandardSurface"), EInterchangeNodeContainerType::FactoryData);

		BaseNodeContainer->AddNode(FunctionCallExpression);
		BaseNodeContainer->SetNodeParentUid(FunctionCallExpressionUid, MaterialFactoryNode->GetUniqueID());

		const FName MaterialFunctionMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionMaterialFunctionCall, MaterialFunction);

		using namespace UE::Interchange::Materials;
		FString StandardSurfaceOrTransmissionPath{ TEXT("MaterialFunction'/Interchange/Functions/MX_StandardSurface.MX_StandardSurface'") };

		if(UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, StandardSurface::Parameters::Transmission))
		{
			StandardSurfaceOrTransmissionPath = TEXT("MaterialFunction'/Interchange/Functions/MX_TransmissionSurface.MX_TransmissionSurface'");
		}

		FunctionCallExpression->AddStringAttribute(
			MaterialFunctionMemberName,
			StandardSurfaceOrTransmissionPath);
		FunctionCallExpression->AddApplyAndFillDelegates<FString>(MaterialFunctionMemberName, UMaterialExpressionMaterialFunctionCall::StaticClass(), MaterialFunctionMemberName);

		auto ConnectMaterialExpressionOutputToInput = [&](const FString & InputName, EMaterialInputType InputType)
		{
			TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, InputType);

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, InputName, FunctionCallExpression->GetUniqueID());

			if(ExpressionFactoryNode.Get<0>())
			{
				UInterchangeShaderPortsAPI::ConnectOuputToInput(FunctionCallExpression, InputName,
																ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}
		};

		using ArgumentsType = TTuple<FString, EMaterialInputType>;
		
		static ArgumentsType ArgumentsPerInputs[]
		{
			ArgumentsType{StandardSurface::Parameters::Base.ToString(), EMaterialInputType::Scalar},                          // Base
			ArgumentsType{StandardSurface::Parameters::BaseColor.ToString(), EMaterialInputType::Color},                      // BaseColor
			ArgumentsType{StandardSurface::Parameters::DiffuseRoughness.ToString(),  EMaterialInputType::Scalar},             // DiffuseRoughness
			ArgumentsType{StandardSurface::Parameters::Metalness.ToString(), EMaterialInputType::Scalar},                     // Metalness
			ArgumentsType{StandardSurface::Parameters::Specular.ToString(), EMaterialInputType::Scalar},                      // Specular
			ArgumentsType{StandardSurface::Parameters::SpecularRoughness.ToString(), EMaterialInputType::Scalar},             // SpecularRoughness
			ArgumentsType{StandardSurface::Parameters::SpecularIOR.ToString(), EMaterialInputType::Scalar},                   // SpecularIOR
			ArgumentsType{StandardSurface::Parameters::SpecularAnisotropy.ToString(), EMaterialInputType::Scalar},            // SpecularAnisotropy
			ArgumentsType{StandardSurface::Parameters::SpecularRotation.ToString(), EMaterialInputType::Scalar},              // SpecularRotation
			ArgumentsType{StandardSurface::Parameters::Subsurface.ToString(), EMaterialInputType::Scalar},                    // Subsurface
			ArgumentsType{StandardSurface::Parameters::SubsurfaceColor.ToString(), EMaterialInputType::Scalar},               // SubsurfaceColor
			ArgumentsType{StandardSurface::Parameters::SubsurfaceRadius.ToString(), EMaterialInputType::Scalar},              // SubsurfaceRadius
			ArgumentsType{StandardSurface::Parameters::SubsurfaceScale.ToString(), EMaterialInputType::Scalar},               // SubsurfaceScale
			ArgumentsType{StandardSurface::Parameters::Sheen.ToString(), EMaterialInputType::Scalar},                         // Sheen
			ArgumentsType{StandardSurface::Parameters::SheenColor.ToString(), EMaterialInputType::Color},                     // SheenColor
			ArgumentsType{StandardSurface::Parameters::SheenRoughness.ToString(), EMaterialInputType::Scalar},                // SheenRoughness
			ArgumentsType{StandardSurface::Parameters::Coat.ToString(), EMaterialInputType::Scalar},                          // Coat
			ArgumentsType{StandardSurface::Parameters::CoatColor.ToString(),  EMaterialInputType::Color},                     // CoatColor
			ArgumentsType{StandardSurface::Parameters::CoatNormal.ToString(),  EMaterialInputType::Vector},                   // CoatNormal
			ArgumentsType{StandardSurface::Parameters::CoatRoughness.ToString(), EMaterialInputType::Scalar},                 // CoatRoughness
			ArgumentsType{StandardSurface::Parameters::ThinFilmThickness.ToString(), EMaterialInputType::Scalar},             // ThinFilmThickness
			ArgumentsType{StandardSurface::Parameters::Emission.ToString(), EMaterialInputType::Scalar},                      // Emission
			ArgumentsType{StandardSurface::Parameters::EmissionColor.ToString(), EMaterialInputType::Color},                  // EmissionColor
			ArgumentsType{StandardSurface::Parameters::Normal.ToString(), EMaterialInputType::Vector},                        // Normal
			ArgumentsType{StandardSurface::Parameters::Tangent.ToString(), EMaterialInputType::Vector},                       // Tangent
			ArgumentsType{StandardSurface::Parameters::Transmission.ToString(),  EMaterialInputType::Scalar},                 // Transmission
			ArgumentsType{StandardSurface::Parameters::TransmissionColor.ToString(), EMaterialInputType::Color},              // TransmissionColor
			ArgumentsType{StandardSurface::Parameters::TransmissionDepth.ToString(), EMaterialInputType::Scalar},             // TransmissionDepth
			ArgumentsType{StandardSurface::Parameters::TransmissionScatter.ToString(), EMaterialInputType::Color},            // TransmissionScatter
			ArgumentsType{StandardSurface::Parameters::TransmissionScatterAnisotropy.ToString(), EMaterialInputType::Scalar}, // TransmissionScatterAnisotropy
			ArgumentsType{StandardSurface::Parameters::TransmissionDispersion.ToString(), EMaterialInputType::Scalar},        // TransmissionDispersion
			ArgumentsType{StandardSurface::Parameters::TransmissionExtraRoughness.ToString(), EMaterialInputType::Scalar},    // TransmissionExtraRoughness
		};

		for(const ArgumentsType & Arguments : ArgumentsPerInputs)
		{
			const FString& InputName = Arguments.Get<0>();
			EMaterialInputType InputType = Arguments.Get<1>();
			ConnectMaterialExpressionOutputToInput(InputName, InputType);
		}
		
		MaterialFactoryNode->ConnectOutputToBaseColor(FunctionCallExpressionUid,  PBR::Parameters::BaseColor.ToString());
		MaterialFactoryNode->ConnectOutputToMetallic(FunctionCallExpressionUid, PBR::Parameters::Metallic.ToString());
		MaterialFactoryNode->ConnectOutputToSpecular(FunctionCallExpressionUid, PBR::Parameters::Specular.ToString());
		MaterialFactoryNode->ConnectOutputToRoughness(FunctionCallExpressionUid, PBR::Parameters::Roughness.ToString());
		MaterialFactoryNode->ConnectOutputToEmissiveColor(FunctionCallExpressionUid, PBR::Parameters::EmissiveColor.ToString());
		MaterialFactoryNode->ConnectOutputToAnisotropy(FunctionCallExpressionUid, PBR::Parameters::Anisotropy.ToString());
		MaterialFactoryNode->ConnectOutputToNormal(FunctionCallExpressionUid, PBR::Parameters::Normal.ToString());
		MaterialFactoryNode->ConnectOutputToTangent(FunctionCallExpressionUid, PBR::Parameters::Tangent.ToString());
		MaterialFactoryNode->ConnectOutputToOpacity(FunctionCallExpressionUid, PBR::Parameters::Opacity.ToString());
		

		// We can't have all shading models at once, so we have to make a choice here
		if(UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, StandardSurface::Parameters::Sheen))
		{
			MaterialFactoryNode->ConnectOutputToFuzzColor(FunctionCallExpressionUid, Sheen::Parameters::SheenColor.ToString());
			MaterialFactoryNode->ConnectOutputToCloth(FunctionCallExpressionUid, Sheen::Parameters::SheenRoughness.ToString());
			MaterialFactoryNode->SetCustomShadingModel(MSM_Cloth);
		}
		else if(UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, StandardSurface::Parameters::Coat))
		{
			MaterialFactoryNode->ConnectOutputToClearCoat(FunctionCallExpressionUid, ClearCoat::Parameters::ClearCoat.ToString());
			MaterialFactoryNode->ConnectOutputToClearCoatRoughness(FunctionCallExpressionUid, ClearCoat::Parameters::ClearCoatRoughness.ToString());
			MaterialFactoryNode->ConnectOutputToClearCoatNormal(FunctionCallExpressionUid, ClearCoat::Parameters::ClearCoatNormal.ToString());
			MaterialFactoryNode->SetCustomShadingModel(MSM_ClearCoat);
		}
		else if(UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, StandardSurface::Parameters::Subsurface))
		{
			MaterialFactoryNode->ConnectOutputToSubsurface(FunctionCallExpressionUid, Subsurface::Parameters::SubsurfaceColor.ToString());
			MaterialFactoryNode->SetCustomShadingModel(MSM_SubsurfaceProfile); //MaterialX subsurface fits more with subsurface profile than subsurface, see: standard_surface_chess_set (King_B/W, Queen_B/W)
		}
		else if (UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, StandardSurface::Parameters::Transmission))
		{
			MaterialFactoryNode->ConnectOutputToTransmissionColor(FunctionCallExpressionUid, ThinTranslucent::Parameters::TransmissionColor.ToString());

			MaterialFactoryNode->SetCustomBlendMode(EBlendMode::BLEND_Translucent);
			MaterialFactoryNode->SetCustomShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
			MaterialFactoryNode->SetCustomTranslucencyLightingMode(ETranslucencyLightingMode::TLM_SurfacePerPixelLighting);
		}
		else
		{
			MaterialFactoryNode->SetCustomShadingModel(MSM_DefaultLit);
		}

		return true;
	}

	return false;
}

bool UInterchangeGenericMaterialPipeline::HandleClearCoat(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials::ClearCoat;

	bool bShadingModelHandled = false;

	// Clear Coat
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::ClearCoat);

		if (bHasInput)
		{
			TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::ClearCoat.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToClearCoat(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	// Clear Coat Roughness
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::ClearCoatRoughness);

		if (bHasInput)
		{
			TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::ClearCoatRoughness.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToClearCoatRoughness(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	// Clear Coat Normal
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::ClearCoatNormal);

		if (bHasInput)
		{
			TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Vector);

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::ClearCoatNormal.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToClearCoatNormal(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	if (bShadingModelHandled)
	{
		MaterialFactoryNode->SetCustomShadingModel(EMaterialShadingModel::MSM_ClearCoat);
	}

	return bShadingModelHandled;
}

bool UInterchangeGenericMaterialPipeline::HandleSubsurface(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials::Subsurface;

	bool bShadingModelHandled = false;

	// Subsurface Color
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::SubsurfaceColor);

		if(bHasInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::SubsurfaceColor.ToString(), MaterialFactoryNode->GetUniqueID());

			if(ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToSubsurface(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	if(bShadingModelHandled)
	{
		MaterialFactoryNode->SetCustomShadingModel(EMaterialShadingModel::MSM_Subsurface);
	}

	return bShadingModelHandled;
}

bool UInterchangeGenericMaterialPipeline::HandleSheen(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials::Sheen;

	bool bShadingModelHandled = false;

	// Sheen Color
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::SheenColor);

		if (bHasInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::SheenColor.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToFuzzColor(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	// Sheen Roughness
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::SheenRoughness);

		if (bHasInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::SheenRoughness.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				UInterchangeMaterialExpressionFactoryNode* InverseSheenRoughnessNode =
						CreateExpressionNode(TEXT("InverseSheenRoughness"), ExpressionFactoryNode.Get<0>()->GetUniqueID(), UMaterialExpressionOneMinus::StaticClass());

				UInterchangeShaderPortsAPI::ConnectOuputToInput(InverseSheenRoughnessNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionOneMinus, Input).ToString(),
					ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());

				MaterialFactoryNode->ConnectToCloth(InverseSheenRoughnessNode->GetUniqueID());
			}

			bShadingModelHandled = true;
		}
	}

	if (bShadingModelHandled)
	{
		MaterialFactoryNode->SetCustomShadingModel(EMaterialShadingModel::MSM_Cloth);
	}

	return bShadingModelHandled;
}

bool UInterchangeGenericMaterialPipeline::HandleThinTranslucent(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials::ThinTranslucent;

	bool bShadingModelHandled = false;

	// Transmission Color
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::TransmissionColor);

		if (bHasInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::TransmissionColor.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToTransmissionColor(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			bShadingModelHandled = true;
		}
	}

	if (bShadingModelHandled)
	{
		MaterialFactoryNode->SetCustomBlendMode(EBlendMode::BLEND_Translucent);
		MaterialFactoryNode->SetCustomShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
		MaterialFactoryNode->SetCustomTranslucencyLightingMode(ETranslucencyLightingMode::TLM_SurfacePerPixelLighting);
	}

	return bShadingModelHandled;
}

void UInterchangeGenericMaterialPipeline::HandleCommonParameters(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials::Common;

	// Two sidedness (ignored for thin translucency as it looks wrong)
	if (!IsThinTranslucentModel(ShaderGraphNode))
	{
		bool bTwoSided = false;
		ShaderGraphNode->GetCustomTwoSided(bTwoSided);
		MaterialFactoryNode->SetCustomTwoSided(bTwoSided);
	}

	// Emissive
	{
		const bool bHasEmissiveInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::EmissiveColor);

		if (bHasEmissiveInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> EmissiveExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::EmissiveColor.ToString(), MaterialFactoryNode->GetUniqueID());

			if (EmissiveExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToEmissiveColor(EmissiveExpressionFactoryNode.Get<0>()->GetUniqueID(), EmissiveExpressionFactoryNode.Get<1>());
			}
		}
	}

	// Normal
	{
		const bool bHasNormalInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::Normal);

		if (bHasNormalInput)
		{
			TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Vector);

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::Normal.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToNormal(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}
		}
	}

	// Opacity
	{
		TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

		const bool bHasOpacityInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::Opacity);

		if (bHasOpacityInput)
		{
			bool bHasSomeTransparency = true;

			float OpacityValue;
			if (ShaderGraphNode->GetFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Parameters::Opacity.ToString()), OpacityValue))
			{
				bHasSomeTransparency = !FMath::IsNearlyEqual(OpacityValue, 1.f);
			}

			if (bHasSomeTransparency)
			{
				TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> OpacityExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::Opacity.ToString(), MaterialFactoryNode->GetUniqueID());

				if (OpacityExpressionFactoryNode.Get<0>())
				{
					MaterialFactoryNode->ConnectOutputToOpacity(OpacityExpressionFactoryNode.Get<0>()->GetUniqueID(), OpacityExpressionFactoryNode.Get<1>());
				}

				using namespace UE::Interchange::InterchangeGenericMaterialPipeline;

				Private::UpdateBlendModeBasedOnOpacityAttributes(ShaderGraphNode, MaterialFactoryNode);
			}
		}
	}

	// Ambient Occlusion
	{
		TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

		const bool bHasOcclusionInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::Occlusion);

		if (bHasOcclusionInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::Occlusion.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToOcclusion(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}
		}
	}

	// Index of Refraction (IOR)
	// We'll lerp between Air IOR (1) and the IOR from the shader graph based on a fresnel, as per UE doc on refraction.
	{
		TGuardValue<EMaterialInputType> InputTypeBeingProcessedGuard(MaterialCreationContext.InputTypeBeingProcessed, EMaterialInputType::Scalar);

		const bool bHasIorInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::IndexOfRefraction);

		if (bHasIorInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Parameters::IndexOfRefraction.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				UInterchangeMaterialExpressionFactoryNode* IORLerp = CreateExpressionNode(TEXT("IORLerp"), ShaderGraphNode->GetUniqueID(), UMaterialExpressionLinearInterpolate::StaticClass());

				const float AirIOR = 1.f;
				const FName ConstAMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionLinearInterpolate, ConstA);
				IORLerp->AddFloatAttribute(ConstAMemberName, AirIOR);
				IORLerp->AddApplyAndFillDelegates<float>(ConstAMemberName, UMaterialExpressionLinearInterpolate::StaticClass(), ConstAMemberName);

				UInterchangeShaderPortsAPI::ConnectOuputToInput(IORLerp, GET_MEMBER_NAME_CHECKED(UMaterialExpressionLinearInterpolate, B).ToString(),
					ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());

				UInterchangeMaterialExpressionFactoryNode* IORFresnel = CreateExpressionNode(TEXT("IORFresnel"), ShaderGraphNode->GetUniqueID(), UMaterialExpressionFresnel::StaticClass());

				UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(IORLerp, GET_MEMBER_NAME_CHECKED(UMaterialExpressionLinearInterpolate, Alpha).ToString(), IORFresnel->GetUniqueID());

				MaterialFactoryNode->ConnectToRefraction(IORLerp->GetUniqueID());
			}
		}
	}
}

void UInterchangeGenericMaterialPipeline::HandleFlattenNormalNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode,
	UInterchangeMaterialExpressionFactoryNode* FlattenNormalFactoryNode)
{
	using namespace UE::Interchange::Materials::Standard::Nodes::FlattenNormal;

	FlattenNormalFactoryNode->SetCustomExpressionClassName(UMaterialExpressionMaterialFunctionCall::StaticClass()->GetName());

	const FName MaterialFunctionMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionMaterialFunctionCall, MaterialFunction);
	FlattenNormalFactoryNode->AddStringAttribute(MaterialFunctionMemberName, TEXT("/Engine/Functions/Engine_MaterialFunctions01/Texturing/FlattenNormal.FlattenNormal"));
	FlattenNormalFactoryNode->AddApplyAndFillDelegates<FString>(MaterialFunctionMemberName, UMaterialExpressionMaterialFunctionCall::StaticClass(), MaterialFunctionMemberName);

	// Normal
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> NormalExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Inputs::Normal.ToString(), FlattenNormalFactoryNode->GetUniqueID());

		if (NormalExpression.Get<0>())
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput(FlattenNormalFactoryNode, TEXT("Normal"),
				NormalExpression.Get<0>()->GetUniqueID(), NormalExpression.Get<1>());
		}
	}

	// Flatness
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> FlatnessExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Inputs::Flatness.ToString(), FlattenNormalFactoryNode->GetUniqueID());

		if (FlatnessExpression.Get<0>())
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput(FlattenNormalFactoryNode, TEXT("Flatness"),
				FlatnessExpression.Get<0>()->GetUniqueID(), FlatnessExpression.Get<1>());
		}
	}
}

void UInterchangeGenericMaterialPipeline::HandleMakeFloat3Node(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode,
	UInterchangeMaterialExpressionFactoryNode* MakeFloat3FactoryNode)
{
	using namespace UE::Interchange::Materials::Standard::Nodes::MakeFloat3;

	MakeFloat3FactoryNode->SetCustomExpressionClassName(UMaterialExpressionMaterialFunctionCall::StaticClass()->GetName());

	const FName MaterialFunctionMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionMaterialFunctionCall, MaterialFunction);
	MakeFloat3FactoryNode->AddStringAttribute(MaterialFunctionMemberName, TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/MakeFloat3.MakeFloat3"));
	MakeFloat3FactoryNode->AddApplyAndFillDelegates<FString>(MaterialFunctionMemberName, UMaterialExpressionMaterialFunctionCall::StaticClass(), MaterialFunctionMemberName);

	TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> RedChannelExpression =
		CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Inputs::X.ToString(), MakeFloat3FactoryNode->GetUniqueID());
	if (RedChannelExpression.Get<0>())
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInput(MakeFloat3FactoryNode, TEXT("X"),
			RedChannelExpression.Get<0>()->GetUniqueID(), RedChannelExpression.Get<1>());
	}

	TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> GreenChannelExpression =
		CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Inputs::Y.ToString(), MakeFloat3FactoryNode->GetUniqueID());
	if (GreenChannelExpression.Get<0>())
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInput(MakeFloat3FactoryNode, TEXT("Y"),
			GreenChannelExpression.Get<0>()->GetUniqueID(), GreenChannelExpression.Get<1>());
	}

	TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> BlueChannelExpression =
		CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Inputs::Z.ToString(), MakeFloat3FactoryNode->GetUniqueID());
	if (BlueChannelExpression.Get<0>())
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInput(MakeFloat3FactoryNode, TEXT("Z"),
			BlueChannelExpression.Get<0>()->GetUniqueID(), BlueChannelExpression.Get<1>());
	}
}

void UInterchangeGenericMaterialPipeline::HandleTextureSampleNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* TextureSampleFactoryNode)
{
	using namespace UE::Interchange::Materials::Standard::Nodes::TextureSample;

	FString TextureUid;
	ShaderNode->GetStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Inputs::Texture.ToString()), TextureUid);

	FString ExpressionClassName;
	FString TextureFactoryUid;

	if (const UInterchangeTextureNode* TextureNode = Cast<const UInterchangeTextureNode>(BaseNodeContainer->GetNode(TextureUid)))
	{
		if (TextureNode->IsA<UInterchangeTextureCubeNode>())
		{
			ExpressionClassName = UMaterialExpressionTextureSampleParameterCube::StaticClass()->GetName();
		}
		else if (TextureNode->IsA<UInterchangeTexture2DArrayNode>())
		{
			ExpressionClassName = UMaterialExpressionTextureSampleParameter2DArray::StaticClass()->GetName();
		}
		else if (TextureNode->IsA<UInterchangeTexture2DNode>())
		{
			ExpressionClassName = UMaterialExpressionTextureSampleParameter2D::StaticClass()->GetName();
		}
		else
		{
			ExpressionClassName = UMaterialExpressionTextureSampleParameter2D::StaticClass()->GetName();
		}

		TArray<FString> TextureTargetNodes;
		TextureNode->GetTargetNodeUids(TextureTargetNodes);

		if (TextureTargetNodes.Num() > 0)
		{
			TextureFactoryUid = TextureTargetNodes[0];
		}

		TextureSampleFactoryNode->SetCustomExpressionClassName(ExpressionClassName);
		TextureSampleFactoryNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Inputs::Texture.ToString()), TextureFactoryUid);

		if (UInterchangeTextureFactoryNode* TextureFactoryNode = Cast<UInterchangeTextureFactoryNode>(BaseNodeContainer->GetFactoryNode(TextureFactoryUid)))
		{
			EMaterialInputType TextureUsage = EMaterialInputType::Unknown;
			TextureFactoryNode->GetAttribute(TEXT("TextureUsage"), TextureUsage);

			const bool bIsOutputLinear = MaterialExpressionCreationContextStack.Top().OutputName.Equals(Outputs::A.ToString());
			const EMaterialInputType DesiredTextureUsage = MaterialCreationContext.InputTypeBeingProcessed == EMaterialInputType::Scalar && bIsOutputLinear ?
																		EMaterialInputType::Unknown : // Alpha channels are always in linear space so ignore them when determining texture usage
																		MaterialCreationContext.InputTypeBeingProcessed;

			if (TextureUsage == EMaterialInputType::Unknown)
			{
				if (DesiredTextureUsage == EMaterialInputType::Vector)
				{
					TextureFactoryNode->SetCustomCompressionSettings(TextureCompressionSettings::TC_Normalmap);
				}
				else if (DesiredTextureUsage == EMaterialInputType::Scalar)
				{
					bool bSRGB;
					if (!TextureNode->GetCustomSRGB(bSRGB))
					{
						// Only set CustomSRGB if it wasn't set by the InterchangeGenericTexturePipeline before
						TextureFactoryNode->SetCustomSRGB(false);
					}
				}

				TextureFactoryNode->SetAttribute(TEXT("TextureUsage"), DesiredTextureUsage);
			}
			else if (TextureUsage != DesiredTextureUsage && DesiredTextureUsage != EMaterialInputType::Unknown)
			{
				UInterchangeResultWarning_Generic* TextureUsageWarning = AddMessage<UInterchangeResultWarning_Generic>();
				TextureUsageWarning->DestinationAssetName = TextureFactoryNode->GetAssetName();
				TextureUsageWarning->AssetType = TextureFactoryNode->GetObjectClass();

				TextureUsageWarning->Text = FText::Format(LOCTEXT("TextureUsageMismatch", "{0} is being used as both {1} and {2} which aren't compatible."),
					FText::FromString(TextureFactoryNode->GetAssetName()), FText::FromString(LexToString(TextureUsage)), FText::FromString(LexToString(DesiredTextureUsage)));

				// Flipping the green channel only makes sense for vector data as it's used to compensate for different handedness.
				// Clear it if we're not gonna be used only as a vector map. This normally happens when a normal map is also used as a color map.
				bool bFlipGreenChannel;
				if (TextureFactoryNode->GetCustombFlipGreenChannel(bFlipGreenChannel))
				{
					TextureFactoryNode->SetCustombFlipGreenChannel(false);
				}
			}
		}
	}
	else
	{
		TextureSampleFactoryNode->SetCustomExpressionClassName(UMaterialExpressionTextureSampleParameter2D::StaticClass()->GetName());
		TextureSampleFactoryNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Inputs::Texture.ToString()), TextureFactoryUid);
	}

	// Coordinates
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> CoordinatesExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Inputs::Coordinates.ToString(), TextureSampleFactoryNode->GetUniqueID());

		if (CoordinatesExpression.Get<0>())
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput(TextureSampleFactoryNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureSample, Coordinates).ToString(),
				CoordinatesExpression.Get<0>()->GetUniqueID(), CoordinatesExpression.Get<1>());
		}
	}
}

void UInterchangeGenericMaterialPipeline::HandleTextureCoordinateNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode,
	UInterchangeMaterialExpressionFactoryNode*& TexCoordFactoryNode)
{
	using namespace UE::Interchange::Materials::Standard;

	TexCoordFactoryNode->SetCustomExpressionClassName(UMaterialExpressionTextureCoordinate::StaticClass()->GetName());

	// Index
	{
		int32 CoordIndex;
		if (ShaderNode->GetInt32Attribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Nodes::TextureCoordinate::Inputs::Index.ToString()), CoordIndex))
		{
			const FName CoordinateIndexMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureCoordinate, CoordinateIndex);
			TexCoordFactoryNode->AddInt32Attribute(CoordinateIndexMemberName, CoordIndex);
			TexCoordFactoryNode->AddApplyAndFillDelegates<int32>(CoordinateIndexMemberName, UMaterialExpressionTextureCoordinate::StaticClass(), CoordinateIndexMemberName);
		}
	}

	// U tiling
	{
		TVariant<FString, FLinearColor, float> UTilingValue = VisitShaderInput(ShaderNode, Nodes::TextureCoordinate::Inputs::UTiling.ToString());

		if (UTilingValue.IsType<float>())
		{
			const FName UTilingMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureCoordinate, UTiling);
			TexCoordFactoryNode->AddFloatAttribute(UTilingMemberName, UTilingValue.Get<float>());
			TexCoordFactoryNode->AddApplyAndFillDelegates<float>(UTilingMemberName, UMaterialExpressionTextureCoordinate::StaticClass(), UTilingMemberName);
		}
	}

	// V tiling
	{
		TVariant<FString, FLinearColor, float> VTilingValue = VisitShaderInput(ShaderNode, Nodes::TextureCoordinate::Inputs::VTiling.ToString());

		if (VTilingValue.IsType<float>())
		{
			const FName VTilingMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureCoordinate, VTiling);
			TexCoordFactoryNode->AddFloatAttribute(VTilingMemberName, VTilingValue.Get<float>());
			TexCoordFactoryNode->AddApplyAndFillDelegates<float>(VTilingMemberName, UMaterialExpressionTextureCoordinate::StaticClass(), VTilingMemberName);
		}
	}

	// Scale
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ScaleExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Nodes::TextureCoordinate::Inputs::Scale.ToString(), TexCoordFactoryNode->GetUniqueID());

		if (ScaleExpression.Get<0>())
		{
			UInterchangeMaterialExpressionFactoryNode* MultiplyExpression =
				CreateExpressionNode(ScaleExpression.Get<0>()->GetDisplayLabel() + TEXT("_Multiply"), TexCoordFactoryNode->GetUniqueID(), UMaterialExpressionMultiply::StaticClass());

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(MultiplyExpression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionMultiply, A).ToString(),
				TexCoordFactoryNode->GetUniqueID());
			UInterchangeShaderPortsAPI::ConnectOuputToInput(MultiplyExpression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionMultiply, B).ToString(),
				ScaleExpression.Get<0>()->GetUniqueID(), ScaleExpression.Get<1>());

			TexCoordFactoryNode = MultiplyExpression;
		}
	}
	
	// Rotate
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> RotateExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Nodes::TextureCoordinate::Inputs::Rotate.ToString(), TexCoordFactoryNode->GetUniqueID());

		if (RotateExpression.Get<0>())
		{
			UInterchangeMaterialExpressionFactoryNode* CallRotatorExpression =
				CreateExpressionNode(RotateExpression.Get<0>()->GetDisplayLabel() + TEXT("_Rotator"), TexCoordFactoryNode->GetUniqueID(), UMaterialExpressionMaterialFunctionCall::StaticClass());

			const FName MaterialFunctionMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionMaterialFunctionCall, MaterialFunction);
			CallRotatorExpression->AddStringAttribute(MaterialFunctionMemberName, TEXT("/Engine/Functions/Engine_MaterialFunctions02/Texturing/CustomRotator.CustomRotator"));
			CallRotatorExpression->AddApplyAndFillDelegates<FString>(MaterialFunctionMemberName, UMaterialExpressionMaterialFunctionCall::StaticClass(), MaterialFunctionMemberName);

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(CallRotatorExpression, TEXT("UVs"), TexCoordFactoryNode->GetUniqueID());
			UInterchangeShaderPortsAPI::ConnectOuputToInput(CallRotatorExpression, TEXT("Rotation Angle (0-1)"), RotateExpression.Get<0>()->GetUniqueID(), RotateExpression.Get<1>());

			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> RotationCenterExpression =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Nodes::TextureCoordinate::Inputs::RotationCenter.ToString(), TexCoordFactoryNode->GetUniqueID());

			if (RotationCenterExpression.Get<0>())
			{
				UInterchangeShaderPortsAPI::ConnectOuputToInput(CallRotatorExpression, TEXT("Rotation Center"), RotationCenterExpression.Get<0>()->GetUniqueID(), RotationCenterExpression.Get<1>());
			}

			TexCoordFactoryNode = CallRotatorExpression;
		}
	}

	// Offset
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> OffsetExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Nodes::TextureCoordinate::Inputs::Offset.ToString(), TexCoordFactoryNode->GetUniqueID());

		if (OffsetExpression.Get<0>())
		{
			UInterchangeMaterialExpressionFactoryNode* AddExpression =
				CreateExpressionNode(OffsetExpression.Get<0>()->GetDisplayLabel() + TEXT("_Add"), TexCoordFactoryNode->GetUniqueID(), UMaterialExpressionAdd::StaticClass());

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(AddExpression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionAdd, A).ToString(),
				TexCoordFactoryNode->GetUniqueID());
			UInterchangeShaderPortsAPI::ConnectOuputToInput(AddExpression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionAdd, B).ToString(),
				OffsetExpression.Get<0>()->GetUniqueID(), OffsetExpression.Get<1>());

			TexCoordFactoryNode = AddExpression;
		}
	}
}

void UInterchangeGenericMaterialPipeline::HandleLerpNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* LerpFactoryNode)
{
	using namespace UE::Interchange::Materials::Standard;

	LerpFactoryNode->SetCustomExpressionClassName(UMaterialExpressionLinearInterpolate::StaticClass()->GetName());

	// A
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ColorAExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Nodes::Lerp::Inputs::A.ToString(), LerpFactoryNode->GetUniqueID());

		if (ColorAExpression.Get<0>())
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput(LerpFactoryNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionLinearInterpolate, A).ToString(),
				ColorAExpression.Get<0>()->GetUniqueID(), ColorAExpression.Get<1>());
		}
	}
	
	// B
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ColorBExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Nodes::Lerp::Inputs::B.ToString(), LerpFactoryNode->GetUniqueID());

		if (ColorBExpression.Get<0>())
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput(LerpFactoryNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionLinearInterpolate, B).ToString(),
				ColorBExpression.Get<0>()->GetUniqueID(), ColorBExpression.Get<1>());
		}
	}
	
	// Factor
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> FactorExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Nodes::Lerp::Inputs::Factor.ToString(), LerpFactoryNode->GetUniqueID());

		if (FactorExpression.Get<0>())
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput(LerpFactoryNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionLinearInterpolate, Alpha).ToString(),
				FactorExpression.Get<0>()->GetUniqueID(), FactorExpression.Get<1>());
		}
	}
}

void UInterchangeGenericMaterialPipeline::HandleMaskNode(const UInterchangeShaderNode* ShaderNode, UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, UInterchangeMaterialExpressionFactoryNode* MaskFactoryNode)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	MaskFactoryNode->SetCustomExpressionClassName(UMaterialExpressionComponentMask::StaticClass()->GetName());

	bool bRChannel = false;
	ShaderNode->GetBooleanAttribute(Mask::Attributes::R, bRChannel);
	bool bGChannel = false;
	ShaderNode->GetBooleanAttribute(Mask::Attributes::G, bGChannel);
	bool bBChannel = false;
	ShaderNode->GetBooleanAttribute(Mask::Attributes::B, bBChannel);
	bool bAChannel = false;
	ShaderNode->GetBooleanAttribute(Mask::Attributes::A, bAChannel);
	bool bIsAnyMaskChannelSet = bRChannel || bGChannel || bBChannel || bAChannel;

	if(bIsAnyMaskChannelSet)
	{
		// R
		{
			const FName RMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionComponentMask, R);
			MaskFactoryNode->AddBooleanAttribute(RMemberName, bRChannel);
			MaskFactoryNode->AddApplyAndFillDelegates<bool>(RMemberName, UMaterialExpressionComponentMask::StaticClass(), RMemberName);
		}

		// G
		{
			const FName GMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionComponentMask, G);
			MaskFactoryNode->AddBooleanAttribute(GMemberName, bGChannel);
			MaskFactoryNode->AddApplyAndFillDelegates<bool>(GMemberName, UMaterialExpressionComponentMask::StaticClass(), GMemberName);
		}

		// B
		{
			const FName BMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionComponentMask, B);
			MaskFactoryNode->AddBooleanAttribute(BMemberName, bBChannel);
			MaskFactoryNode->AddApplyAndFillDelegates<bool>(BMemberName, UMaterialExpressionComponentMask::StaticClass(), BMemberName);
		}

		// A
		{
			const FName AMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionComponentMask, A);
			MaskFactoryNode->AddBooleanAttribute(AMemberName, bAChannel);
			MaskFactoryNode->AddApplyAndFillDelegates<bool>(AMemberName, UMaterialExpressionComponentMask::StaticClass(), AMemberName);
		}
	}

	// Input
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> InputExpression =
			CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, Mask::Inputs::Input.ToString(), MaskFactoryNode->GetUniqueID());

		if(InputExpression.Get<0>())
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput(MaskFactoryNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionComponentMask, Input).ToString(),
				InputExpression.Get<0>()->GetUniqueID(), InputExpression.Get<1>());
		}
	}
}

UInterchangeMaterialExpressionFactoryNode* UInterchangeGenericMaterialPipeline::CreateMaterialExpressionForShaderNode(UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode,
	const UInterchangeShaderNode* ShaderNode, const FString& ParentUid)
{
	using namespace UE::Interchange::Materials::Standard;

	// If we recognize the shader node type
	// - Create material expression for specific node type
	//
	// If we don't recognize the shader node type
	// - Create material expression by trying to match the node type to a material expression class name

	const FString MaterialExpressionUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(ShaderNode->GetUniqueID());

	UInterchangeMaterialExpressionFactoryNode* MaterialExpression = Cast<UInterchangeMaterialExpressionFactoryNode>(BaseNodeContainer->GetFactoryNode(MaterialExpressionUid));
	if (MaterialExpression != nullptr)
	{
		return MaterialExpression;
	}

	if (const UInterchangeFunctionCallShaderNode* FunctionCallShaderNode = Cast<UInterchangeFunctionCallShaderNode>(ShaderNode))
	{
		UInterchangeMaterialFunctionCallExpressionFactoryNode* MaterialFunctionCallExpression = NewObject<UInterchangeMaterialFunctionCallExpressionFactoryNode>(BaseNodeContainer);
		
		FString MaterialFunctionUid;
		if (FunctionCallShaderNode->GetCustomMaterialFunction(MaterialFunctionUid))
		{
			const FString MaterialFunctionFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(MaterialFunctionUid);
			MaterialFunctionCallExpression->SetCustomMaterialFunctionDependency(MaterialFunctionFactoryNodeUid);
			MaterialFunctionCallExpression->AddFactoryDependencyUid(MaterialFunctionFactoryNodeUid);
		}

		MaterialExpression = MaterialFunctionCallExpression;
	}
	else
	{
		MaterialExpression = NewObject<UInterchangeMaterialExpressionFactoryNode>(BaseNodeContainer);
	}

	FString ShaderType;
	ShaderNode->GetCustomShaderType(ShaderType);

	MaterialExpression->InitializeNode(MaterialExpressionUid, ShaderNode->GetDisplayLabel(), EInterchangeNodeContainerType::FactoryData);
	BaseNodeContainer->AddNode(MaterialExpression);

	if (*ShaderType == Nodes::FlattenNormal::Name)
	{
		HandleFlattenNormalNode(ShaderNode, MaterialFactoryNode, MaterialExpression);
	}
	else if (*ShaderType == Nodes::MakeFloat3::Name)
	{
		HandleMakeFloat3Node(ShaderNode, MaterialFactoryNode, MaterialExpression);
	}
	else if (*ShaderType == Nodes::Lerp::Name)
	{
		HandleLerpNode(ShaderNode, MaterialFactoryNode, MaterialExpression);
	}
	else if(*ShaderType == Nodes::Mask::Name)
	{
		HandleMaskNode(ShaderNode, MaterialFactoryNode, MaterialExpression);
	}
	else if (*ShaderType == Nodes::TextureCoordinate::Name)
	{
		HandleTextureCoordinateNode(ShaderNode, MaterialFactoryNode, MaterialExpression);
	}
	else if (*ShaderType == Nodes::TextureSample::Name)
	{
		HandleTextureSampleNode(ShaderNode, MaterialFactoryNode, MaterialExpression);
	}
	else
	{
		const FString ExpressionClassName = TEXT("MaterialExpression") + ShaderType;
		MaterialExpression->SetCustomExpressionClassName(ExpressionClassName);

		TArray<FString> Inputs;
		UInterchangeShaderPortsAPI::GatherInputs(ShaderNode, Inputs);

		for (const FString& InputName : Inputs)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> InputExpression =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderNode, InputName, MaterialExpressionUid);

			if (InputExpression.Get<0>())
			{
				UInterchangeShaderPortsAPI::ConnectOuputToInput(MaterialExpression, InputName, InputExpression.Get<0>()->GetUniqueID(), InputExpression.Get<1>());
			}
		}
	}

	if (!ParentUid.IsEmpty())
	{
		BaseNodeContainer->SetNodeParentUid(MaterialExpressionUid, ParentUid);
	}

	MaterialExpression->AddTargetNodeUid(ShaderNode->GetUniqueID());

	if (*ShaderType == Nodes::TextureSample::Name)
	{
		FString TextureUid;
		ShaderNode->GetStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Nodes::TextureSample::Inputs::Texture.ToString()), TextureUid);

		// Make the material factory node have a dependency on the texture factory node so that the texture asset gets created first
		if (const UInterchangeTextureNode* TextureNode = Cast<const UInterchangeTextureNode>(BaseNodeContainer->GetNode(TextureUid)))
		{
			TArray<FString> TextureNodeTargets;
			TextureNode->GetTargetNodeUids(TextureNodeTargets);

			if (TextureNodeTargets.Num() > 0)
			{
				FString TextureFactoryNodeUid = TextureNodeTargets[0];

				if (BaseNodeContainer->IsNodeUidValid(TextureFactoryNodeUid))
				{
					TArray<FString> FactoryDependencies;
					MaterialFactoryNode->GetFactoryDependencies(FactoryDependencies);
					if (!FactoryDependencies.Contains(TextureFactoryNodeUid))
					{
						MaterialFactoryNode->AddFactoryDependencyUid(TextureFactoryNodeUid);
					}
				}
			}
		}
	}

	return MaterialExpression;
}

UInterchangeMaterialExpressionFactoryNode* UInterchangeGenericMaterialPipeline::CreateExpressionNode(const FString& ExpressionName, const FString& ParentUid, UClass* MaterialExpressionClass)
{
	const FString MaterialExpressionUid = ParentUid + TEXT("\\") + ExpressionName;

	UInterchangeMaterialExpressionFactoryNode* MaterialExpressionFactoryNode = NewObject<UInterchangeMaterialExpressionFactoryNode>(BaseNodeContainer);
	MaterialExpressionFactoryNode->SetCustomExpressionClassName(MaterialExpressionClass->GetName());
	MaterialExpressionFactoryNode->InitializeNode(MaterialExpressionUid, ExpressionName, EInterchangeNodeContainerType::FactoryData);
	BaseNodeContainer->AddNode(MaterialExpressionFactoryNode);
	BaseNodeContainer->SetNodeParentUid(MaterialExpressionUid, ParentUid);

	return MaterialExpressionFactoryNode;
}

UInterchangeMaterialExpressionFactoryNode* UInterchangeGenericMaterialPipeline::CreateScalarParameterExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid)
{
	UInterchangeMaterialExpressionFactoryNode* MaterialExpressionFactoryNode = CreateExpressionNode(InputName, ParentUid, UMaterialExpressionScalarParameter::StaticClass());

	float InputValue;
	if (ShaderNode->GetFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue))
	{
		const FName DefaultValueMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionScalarParameter, DefaultValue);
		MaterialExpressionFactoryNode->AddFloatAttribute(DefaultValueMemberName, InputValue);
		MaterialExpressionFactoryNode->AddApplyAndFillDelegates<float>(DefaultValueMemberName, UMaterialExpressionScalarParameter::StaticClass(), DefaultValueMemberName);
	}

	return MaterialExpressionFactoryNode;
}

UInterchangeMaterialExpressionFactoryNode* UInterchangeGenericMaterialPipeline::CreateVectorParameterExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid)
{
	UInterchangeMaterialExpressionFactoryNode* MaterialExpressionFactoryNode = CreateExpressionNode(InputName, ParentUid, UMaterialExpressionVectorParameter::StaticClass());

	FLinearColor InputValue;
	if (ShaderNode->GetLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue))
	{
		const FName DefaultValueMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionVectorParameter, DefaultValue);
		MaterialExpressionFactoryNode->AddLinearColorAttribute(DefaultValueMemberName, InputValue);
		MaterialExpressionFactoryNode->AddApplyAndFillDelegates<FLinearColor>(DefaultValueMemberName, UMaterialExpressionVectorParameter::StaticClass(), DefaultValueMemberName);
	}

	return MaterialExpressionFactoryNode;
}

UInterchangeMaterialExpressionFactoryNode* UInterchangeGenericMaterialPipeline::CreateVector2ParameterExpression(const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid)
{
	FVector2f InputValue;
	if (ShaderNode->GetAttribute<FVector2f>(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue))
	{
		UInterchangeMaterialExpressionFactoryNode* VectorParameterFactoryNode = CreateExpressionNode(InputName, ParentUid, UMaterialExpressionVectorParameter::StaticClass());

		const FName DefaultValueMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionVectorParameter, DefaultValue);
		VectorParameterFactoryNode->AddLinearColorAttribute(DefaultValueMemberName, FLinearColor(InputValue.X, InputValue.Y, 0.f));
		VectorParameterFactoryNode->AddApplyAndFillDelegates<FLinearColor>(DefaultValueMemberName, UMaterialExpressionVectorParameter::StaticClass(), DefaultValueMemberName);

		// Defaults to R&G
		UInterchangeMaterialExpressionFactoryNode* ComponentMaskFactoryNode = CreateExpressionNode(InputName + TEXT("_Mask"), ParentUid, UMaterialExpressionComponentMask::StaticClass());

		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ComponentMaskFactoryNode, GET_MEMBER_NAME_CHECKED(UMaterialExpressionComponentMask, Input).ToString(),
			VectorParameterFactoryNode->GetUniqueID() );

		return ComponentMaskFactoryNode;
	}

	return nullptr;
}

TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> UInterchangeGenericMaterialPipeline::CreateMaterialExpressionForInput(UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode, const UInterchangeShaderNode* ShaderNode, const FString& InputName, const FString& ParentUid)
{
	// Make sure we don't create an expression for an input if it already has one
	if (UInterchangeShaderPortsAPI::HasInput(MaterialFactoryNode, *InputName))
	{
		return TTuple<UInterchangeMaterialExpressionFactoryNode*, FString>{};
	}

	// If we have a connection
	// - Create material expression for the connected shader node
	//
	// If we don't have a connection
	// - Create material expression for the input value

	UInterchangeMaterialExpressionFactoryNode* MaterialExpressionFactoryNode = nullptr;

	int32 ExpressionContextIndex = MaterialExpressionCreationContextStack.AddDefaulted();

	FString ConnectedShaderNodeUid;
	if (UInterchangeShaderPortsAPI::GetInputConnection(ShaderNode, InputName, ConnectedShaderNodeUid, MaterialExpressionCreationContextStack[ExpressionContextIndex].OutputName))
	{
		if (const UInterchangeShaderNode* ConnectedShaderNode = Cast<const UInterchangeShaderNode>(BaseNodeContainer->GetNode(ConnectedShaderNodeUid)))
		{
			MaterialExpressionFactoryNode = CreateMaterialExpressionForShaderNode(MaterialFactoryNode, ConnectedShaderNode, ParentUid);
		}
	}
	else
	{
		switch(UInterchangeShaderPortsAPI::GetInputType(ShaderNode, InputName))
		{
		case UE::Interchange::EAttributeTypes::Float:
			MaterialExpressionFactoryNode = CreateScalarParameterExpression(ShaderNode, InputName, ParentUid);
			break;
		case UE::Interchange::EAttributeTypes::LinearColor:
			MaterialExpressionFactoryNode = CreateVectorParameterExpression(ShaderNode, InputName, ParentUid);
			break;
		case UE::Interchange::EAttributeTypes::Vector2f:
			MaterialExpressionFactoryNode = CreateVector2ParameterExpression(ShaderNode, InputName, ParentUid);
			break;
		}
	}

	TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> Result {MaterialExpressionFactoryNode, MaterialExpressionCreationContextStack[ExpressionContextIndex].OutputName};
	MaterialExpressionCreationContextStack.Pop();

	return Result;
}

UInterchangeMaterialFactoryNode* UInterchangeGenericMaterialPipeline::CreateMaterialFactoryNode(const UInterchangeShaderGraphNode* ShaderGraphNode)
{
	UInterchangeMaterialFactoryNode* MaterialFactoryNode = Cast<UInterchangeMaterialFactoryNode>( CreateBaseMaterialFactoryNode(ShaderGraphNode, UInterchangeMaterialFactoryNode::StaticClass()) );

	if(HandleStandardSurfaceModel(ShaderGraphNode, MaterialFactoryNode))
	{
		return MaterialFactoryNode;
	}

	// Handle the case where the material will be connected through the material attributes input
	if (HandleBxDFInput(ShaderGraphNode, MaterialFactoryNode))
	{
		// No need to proceed any further
		return MaterialFactoryNode;
	}

	if (HandleUnlitModel(ShaderGraphNode, MaterialFactoryNode))
	{
		// No need to proceed any further
		return MaterialFactoryNode;
	}

	if (!HandlePhongModel(ShaderGraphNode, MaterialFactoryNode))
	{
		HandleLambertModel(ShaderGraphNode, MaterialFactoryNode);
	}

	HandlePBRModel(ShaderGraphNode, MaterialFactoryNode); // Always process the PBR parameters. If they were already assigned from Phong or Lambert, they will be ignored.
	
	// Can't have different shading models
	// Favor translucency over coats (clear coat, sheen, etc.) since it tends to have a bigger impact visually
	if (!HandleThinTranslucent(ShaderGraphNode, MaterialFactoryNode))
	{
		if (!HandleClearCoat(ShaderGraphNode, MaterialFactoryNode))
		{
			if(!HandleSheen(ShaderGraphNode, MaterialFactoryNode))
			{
				HandleSubsurface(ShaderGraphNode, MaterialFactoryNode);
			}
		}
	}

	HandleCommonParameters(ShaderGraphNode, MaterialFactoryNode);

	return MaterialFactoryNode;
}

UInterchangeMaterialInstanceFactoryNode* UInterchangeGenericMaterialPipeline::CreateMaterialInstanceFactoryNode(const UInterchangeShaderGraphNode* ShaderGraphNode)
{
	UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode =
		Cast<UInterchangeMaterialInstanceFactoryNode>( CreateBaseMaterialFactoryNode(ShaderGraphNode, UInterchangeMaterialInstanceFactoryNode::StaticClass()) );

	if (UMaterialInterface* ParentMaterialObj = Cast<UMaterialInterface>(ParentMaterial.TryLoad()))
	{
		MaterialInstanceFactoryNode->SetCustomParent(ParentMaterialObj->GetPathName());
	}
	else if (IsThinTranslucentModel(ShaderGraphNode))
	{
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/ThinTranslucentMaterial.ThinTranslucentMaterial'"));
	}
	else if (IsClearCoatModel(ShaderGraphNode))
	{
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/ClearCoatMaterial.ClearCoatMaterial'"));
	}
	else if (IsSheenModel(ShaderGraphNode))
	{
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/SheenMaterial.SheenMaterial'"));
	}
	else if(IsSubsurfaceModel(ShaderGraphNode))
	{
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/SubsurfaceMaterial.SubsurfaceMaterial'"));
	}
	else if (IsPBRModel(ShaderGraphNode))
	{
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/PBRSurfaceMaterial.PBRSurfaceMaterial'"));
	}
	else if (IsPhongModel(ShaderGraphNode))
	{
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/PhongSurfaceMaterial.PhongSurfaceMaterial'"));
	}
	else if (IsLambertModel(ShaderGraphNode))
	{
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/LambertSurfaceMaterial.LambertSurfaceMaterial'"));
	}
	else if (IsUnlitModel(ShaderGraphNode))
	{
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/UnlitMaterial.UnlitMaterial'"));
	}
	else
	{
		// Default to PBR
		MaterialInstanceFactoryNode->SetCustomParent(TEXT("Material'/Interchange/Materials/PBRSurfaceMaterial.PBRSurfaceMaterial'"));
	}

#if WITH_EDITOR
	const UClass* MaterialClass = IsRunningGame() ? UMaterialInstanceDynamic::StaticClass() : UMaterialInstanceConstant::StaticClass();
	MaterialInstanceFactoryNode->SetCustomInstanceClassName(MaterialClass->GetPathName());
#else
	MaterialInstanceFactoryNode->SetCustomInstanceClassName(UMaterialInstanceDynamic::StaticClass()->GetPathName());
#endif

	TArray<FString> Inputs;
	UInterchangeShaderPortsAPI::GatherInputs(ShaderGraphNode, Inputs);

	for (const FString& InputName : Inputs)
	{
		TVariant<FString, FLinearColor, float> InputValue;

		FString ConnectedShaderNodeUid;
		FString OutputName;
		if (UInterchangeShaderPortsAPI::GetInputConnection(ShaderGraphNode, InputName, ConnectedShaderNodeUid, OutputName))
		{
			if (const UInterchangeShaderNode* ConnectedShaderNode = Cast<const UInterchangeShaderNode>(BaseNodeContainer->GetNode(ConnectedShaderNodeUid)))
			{
				InputValue = VisitShaderNode(ConnectedShaderNode);
			}
		}
		else
		{
			switch(UInterchangeShaderPortsAPI::GetInputType(ShaderGraphNode, InputName))
			{
			case UE::Interchange::EAttributeTypes::Float:
				{
					float AttributeValue = 0.f;
					ShaderGraphNode->GetFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), AttributeValue);
					InputValue.Set<float>(AttributeValue);
				}
				break;
			case UE::Interchange::EAttributeTypes::LinearColor:
				{
					FLinearColor AttributeValue = FLinearColor::White;
					ShaderGraphNode->GetLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), AttributeValue);
					InputValue.Set<FLinearColor>(AttributeValue);
				}
				break;
			}
		}

		if (InputValue.IsType<float>())
		{
			MaterialInstanceFactoryNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue.Get<float>());
		}
		else if (InputValue.IsType<FLinearColor>())
		{
			MaterialInstanceFactoryNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue.Get<FLinearColor>());
		}
		else if (InputValue.IsType<FString>())
		{
			const FString MapName(InputName + TEXT("Map"));
			MaterialInstanceFactoryNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(MapName), InputValue.Get<FString>());

			const FString MapWeightName(MapName + TEXT("Weight"));
			MaterialInstanceFactoryNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(MapWeightName), 1.f);
		}
	}

	return MaterialInstanceFactoryNode;
}

TVariant<FString, FLinearColor, float> UInterchangeGenericMaterialPipeline::VisitShaderNode(const UInterchangeShaderNode* ShaderNode) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	TVariant<FString, FLinearColor, float> Result;

	FString ShaderType;
	if (ShaderNode->GetCustomShaderType(ShaderType))
	{
		if (*ShaderType == TextureSample::Name)
		{
			return VisitTextureSampleNode(ShaderNode);
		}
		else if (*ShaderType == Lerp::Name)
		{
			return VisitLerpNode(ShaderNode);
		}
		else if (*ShaderType == Multiply::Name)
		{
			return VisitMultiplyNode(ShaderNode);
		}
		else if (*ShaderType == OneMinus::Name)
		{
			return VisitOneMinusNode(ShaderNode);
		}
	}

	{
		TArray<FString> Inputs;
		UInterchangeShaderPortsAPI::GatherInputs(ShaderNode, Inputs);

		if (Inputs.Num() > 0)
		{
			const FString& InputName = Inputs[0];
			Result = VisitShaderInput(ShaderNode, InputName);
		}
	}

	return Result;
}

TVariant<FString, FLinearColor, float> UInterchangeGenericMaterialPipeline::VisitShaderInput(const UInterchangeShaderNode* ShaderNode, const FString& InputName) const
{
	TVariant<FString, FLinearColor, float> Result;

	FString ConnectedShaderNodeUid;
	FString OutputName;
	if (UInterchangeShaderPortsAPI::GetInputConnection(ShaderNode, InputName, ConnectedShaderNodeUid, OutputName))
	{
		if (const UInterchangeShaderNode* ConnectedShaderNode = Cast<const UInterchangeShaderNode>(BaseNodeContainer->GetNode(ConnectedShaderNodeUid)))
		{
			Result = VisitShaderNode(ConnectedShaderNode);
		}
	}
	else
	{
		switch(UInterchangeShaderPortsAPI::GetInputType(ShaderNode, InputName))
		{
		case UE::Interchange::EAttributeTypes::Float:
			{
				float InputValue = 0.f;
				ShaderNode->GetFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue);
				Result.Set<float>(InputValue);
			}
			break;
		case UE::Interchange::EAttributeTypes::LinearColor:
			{
				FLinearColor InputValue = FLinearColor::White;
				ShaderNode->GetLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue);
				Result.Set<FLinearColor>(InputValue);
			}
			break;
		}
	}

	return Result;
}

TVariant<FString, FLinearColor, float> UInterchangeGenericMaterialPipeline::VisitLerpNode(const UInterchangeShaderNode* ShaderNode) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	TVariant<FString, FLinearColor, float> ResultA = VisitShaderInput(ShaderNode, Lerp::Inputs::A.ToString());
	TVariant<FString, FLinearColor, float> ResultB = VisitShaderInput(ShaderNode, Lerp::Inputs::B.ToString());

	TVariant<FString, FLinearColor, float> ResultFactor = VisitShaderInput(ShaderNode, Lerp::Inputs::Factor.ToString());

	bool bResultAIsStrongest = true;

	if (ResultFactor.IsType<float>())
	{
		const float Factor = ResultFactor.Get<float>();
		bResultAIsStrongest = (Factor <= 0.5f);

		// Bake the lerp into a single value
		if (!ResultA.IsType<FString>() && !ResultB.IsType<FString>())
		{
			if (ResultA.IsType<float>() && ResultB.IsType<float>())
			{
				const float ValueA = ResultA.Get<float>();
				const float ValueB = ResultB.Get<float>();

				TVariant<FString, FLinearColor, float> Result;
				Result.Set<float>(FMath::Lerp(ValueA, ValueB, Factor));
				return Result;
			}
			else if (ResultA.IsType<FLinearColor>() && ResultB.IsType<FLinearColor>())
			{
				const FLinearColor ValueA = ResultA.Get<FLinearColor>();
				const FLinearColor ValueB = ResultB.Get<FLinearColor>();

				TVariant<FString, FLinearColor, float> Result;
				Result.Set<FLinearColor>(FMath::Lerp(ValueA, ValueB, Factor));
				return Result;
			}
		}
	}

	if (bResultAIsStrongest)
	{
		return ResultA;
	}
	else
	{
		return ResultB;
	}
}

TVariant<FString, FLinearColor, float> UInterchangeGenericMaterialPipeline::VisitMultiplyNode(const UInterchangeShaderNode* ShaderNode) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	TVariant<FString, FLinearColor, float> ResultA = VisitShaderInput(ShaderNode, Lerp::Inputs::A.ToString());
	TVariant<FString, FLinearColor, float> ResultB = VisitShaderInput(ShaderNode, Lerp::Inputs::B.ToString());

	// Bake the multiply into a single value if possible
	if (!ResultA.IsType<FString>() && !ResultB.IsType<FString>())
	{
		if (ResultA.IsType<float>() && ResultB.IsType<float>())
		{
			const float ValueA = ResultA.Get<float>();
			const float ValueB = ResultB.Get<float>();

			TVariant<FString, FLinearColor, float> Result;
			Result.Set<float>(ValueA * ValueB);
			return Result;
		}
		else if (ResultA.IsType<FLinearColor>() && ResultB.IsType<FLinearColor>())
		{
			const FLinearColor ValueA = ResultA.Get<FLinearColor>();
			const FLinearColor ValueB = ResultB.Get<FLinearColor>();

			TVariant<FString, FLinearColor, float> Result;
			Result.Set<FLinearColor>(ValueA * ValueB);
			return Result;
		}
		else if (ResultA.IsType<FLinearColor>() && ResultB.IsType<float>())
		{
			const FLinearColor ValueA = ResultA.Get<FLinearColor>();
			const float ValueB = ResultB.Get<float>();

			TVariant<FString, FLinearColor, float> Result;
			Result.Set<FLinearColor>(ValueA * ValueB);
			return Result;
		}
		else if (ResultA.IsType<float>() && ResultB.IsType<FLinearColor>())
		{
			const float ValueA = ResultA.Get<float>();
			const FLinearColor ValueB = ResultB.Get<FLinearColor>();

			TVariant<FString, FLinearColor, float> Result;
			Result.Set<FLinearColor>(ValueA * ValueB);
			return Result;
		}
	}

	return ResultA;
}

TVariant<FString, FLinearColor, float> UInterchangeGenericMaterialPipeline::VisitOneMinusNode(const UInterchangeShaderNode* ShaderNode) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	TVariant<FString, FLinearColor, float> ResultInput = VisitShaderInput(ShaderNode, OneMinus::Inputs::Input.ToString());

	if (ResultInput.IsType<FLinearColor>())
	{
		const FLinearColor Value = ResultInput.Get<FLinearColor>();

		TVariant<FString, FLinearColor, float> Result;
		Result.Set<FLinearColor>(FLinearColor::White - Value);
		return Result;
	}
	else if (ResultInput.IsType<float>())
	{
		const float Value = ResultInput.Get<float>();

		TVariant<FString, FLinearColor, float> Result;
		Result.Set<float>(1.f - Value);
		return Result;
	}

	return ResultInput;
}

TVariant<FString, FLinearColor, float> UInterchangeGenericMaterialPipeline::VisitTextureSampleNode(const UInterchangeShaderNode* ShaderNode) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	TVariant<FString, FLinearColor, float> Result;

	FString TextureUid;
	if (ShaderNode->GetStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSample::Inputs::Texture.ToString()), TextureUid))
	{
		if (!TextureUid.IsEmpty())
		{
			FString TextureFactoryUid;
			if (const UInterchangeTextureNode* TextureNode = Cast<const UInterchangeTextureNode>(BaseNodeContainer->GetNode(TextureUid)))
			{
				TArray<FString> TextureTargetNodes;
				TextureNode->GetTargetNodeUids(TextureTargetNodes);

				if (TextureTargetNodes.Num() > 0)
				{
					TextureFactoryUid = TextureTargetNodes[0];
				}
			}

			Result.Set<FString>(TextureFactoryUid);
		}
	}

	return Result;
}

bool UInterchangeGenericMaterialPipeline::HandleBxDFInput(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials;

	if (!ShaderGraphNode || !UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Common::Parameters::BxDF))
	{
		return false;
	}

	TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
		CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Common::Parameters::BxDF.ToString(), MaterialFactoryNode->GetUniqueID());
	ensure(ExpressionFactoryNode.Get<0>());

	if (ExpressionFactoryNode.Get<0>())
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInput(MaterialFactoryNode, Common::Parameters::BxDF.ToString(), ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
	}

	// Make sure the bUseMaterialAttributes property of the material is set to true
	static const FName UseMaterialAttributesMemberName = GET_MEMBER_NAME_CHECKED(UMaterial, bUseMaterialAttributes);

	MaterialFactoryNode->AddBooleanAttribute(UseMaterialAttributesMemberName, true);
	MaterialFactoryNode->AddApplyAndFillDelegates<FString>(UseMaterialAttributesMemberName, UMaterialExpressionMaterialFunctionCall::StaticClass(), UseMaterialAttributesMemberName);

	return true;
}

UInterchangeMaterialFunctionFactoryNode* UInterchangeGenericMaterialPipeline::CreateMaterialFunctionFactoryNode(const UInterchangeShaderGraphNode* ShaderGraphNode)
{
	UInterchangeMaterialFunctionFactoryNode* FactoryNode = Cast<UInterchangeMaterialFunctionFactoryNode>(CreateBaseMaterialFactoryNode(ShaderGraphNode, UInterchangeMaterialFunctionFactoryNode::StaticClass()));
	
	TArray<FString> InputNames;
	UInterchangeShaderPortsAPI::GatherInputs(ShaderGraphNode, InputNames);

	for (const FString& InputName : InputNames)
	{
		TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
			CreateMaterialExpressionForInput(FactoryNode, ShaderGraphNode, InputName, FactoryNode->GetUniqueID());

		if (ExpressionFactoryNode.Get<0>())
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput(FactoryNode, InputName, ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
		}
	}

	return FactoryNode;
}

bool UInterchangeGenericMaterialPipeline::IsUnlitModel(const UInterchangeShaderGraphNode* ShaderGraphNode) const
{
	using namespace UE::Interchange::Materials::Unlit;

	const bool bHasUnlitColorInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Parameters::UnlitColor);

	return bHasUnlitColorInput;
}

bool UInterchangeGenericMaterialPipeline::HandleUnlitModel(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialFactoryNode* MaterialFactoryNode)
{
	using namespace UE::Interchange::Materials;

	bool bShadingModelHandled = false;

	// Unlit Color
	{
		const bool bHasInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Unlit::Parameters::UnlitColor);

		if (bHasInput)
		{
			TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> ExpressionFactoryNode =
				CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Unlit::Parameters::UnlitColor.ToString(), MaterialFactoryNode->GetUniqueID());

			if (ExpressionFactoryNode.Get<0>())
			{
				MaterialFactoryNode->ConnectOutputToEmissiveColor(ExpressionFactoryNode.Get<0>()->GetUniqueID(), ExpressionFactoryNode.Get<1>());
			}

			//gltf allows unlit color to be also translucent:
			{
				const bool bHasOpacityInput = UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, Common::Parameters::Opacity);

				if (bHasOpacityInput)
				{
					TTuple<UInterchangeMaterialExpressionFactoryNode*, FString> OpacityExpressionFactoryNode =
						CreateMaterialExpressionForInput(MaterialFactoryNode, ShaderGraphNode, Common::Parameters::Opacity.ToString(), MaterialFactoryNode->GetUniqueID());

					if (OpacityExpressionFactoryNode.Get<0>())
					{
						MaterialFactoryNode->ConnectOutputToOpacity(OpacityExpressionFactoryNode.Get<0>()->GetUniqueID(), OpacityExpressionFactoryNode.Get<1>());
					}

					using namespace UE::Interchange::InterchangeGenericMaterialPipeline;

					Private::UpdateBlendModeBasedOnOpacityAttributes(ShaderGraphNode, MaterialFactoryNode);
				}
			}

			bShadingModelHandled = true;
		}
	}

	if (bShadingModelHandled)
	{
		MaterialFactoryNode->SetCustomShadingModel(EMaterialShadingModel::MSM_Unlit);
	}

	return bShadingModelHandled;
}

#undef LOCTEXT_NAMESPACE
