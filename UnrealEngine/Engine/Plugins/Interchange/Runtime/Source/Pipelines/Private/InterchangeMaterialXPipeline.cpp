// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeMaterialXPipeline.h"

#include "InterchangePipelineLog.h"

#include "InterchangeManager.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeMaterialFactoryNode.h"

#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Misc/PackageName.h"

TMap<FString, EInterchangeMaterialXShaders> UInterchangeMaterialXPipeline::PathToEnumMapping
{
	{TEXT("/Interchange/Functions/MX_StandardSurface.MX_StandardSurface"),EInterchangeMaterialXShaders::StandardSurface},
	{TEXT("/Interchange/Functions/MX_TransmissionSurface.MX_TransmissionSurface"),EInterchangeMaterialXShaders::StandardSurfaceTransmission},
	{TEXT("/Interchange/Functions/MX_SurfaceUnlit.MX_SurfaceUnlit"),EInterchangeMaterialXShaders::SurfaceUnlit},
	{TEXT("/Interchange/Functions/MX_UsdPreviewSurface.MX_UsdPreviewSurface"),EInterchangeMaterialXShaders::UsdPreviewSurface},
};

UInterchangeMaterialXPipeline::UInterchangeMaterialXPipeline()
	: MaterialXSettings(UMaterialXPipelineSettings::StaticClass()->GetDefaultObject< UMaterialXPipelineSettings>())
{
	for (const TPair<EInterchangeMaterialXShaders, FSoftObjectPath>& Entry : MaterialXSettings->PredefinedSurfaceShaders)
	{
		PathToEnumMapping.FindOrAdd(Entry.Value.GetAssetPathString(), Entry.Key);
	}
}

void UInterchangeMaterialXPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

	if (!MaterialXSettings->AreRequiredPackagesLoaded())
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericMaterialPipeline: Some required packages are missing. Material import might be wrong"));
	}
}

void UInterchangeMaterialXPipeline::ExecutePipeline(UInterchangeBaseNodeContainer* NodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas)
{
	Super::ExecutePipeline(NodeContainer, InSourceDatas);

#if WITH_EDITOR
	auto UpdateMaterialXNodes = [this](const FString& NodeUid, UInterchangeMaterialFunctionCallExpressionFactoryNode* FactorNode)
	{
		const FString MaterialFunctionMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionMaterialFunctionCall, MaterialFunction).ToString();

		FString MaterialFunctionPath;
		if (FactorNode->GetStringAttribute(MaterialFunctionMemberName, MaterialFunctionPath))
		{
			if (const EInterchangeMaterialXShaders* EnumPtr = PathToEnumMapping.Find(MaterialFunctionPath))
			{
				FactorNode->AddStringAttribute(MaterialFunctionMemberName, MaterialXSettings->GetAssetPathString(*EnumPtr));
			}
		}
	};

	//Find all translated node we need for this pipeline
	NodeContainer->IterateNodesOfType<UInterchangeMaterialFunctionCallExpressionFactoryNode>(UpdateMaterialXNodes);
#endif
}

bool UMaterialXPipelineSettings::AreRequiredPackagesLoaded()
{
	auto ArePackagesLoaded = [&](const TMap<EInterchangeMaterialXShaders, FSoftObjectPath>& ObjectPaths)
	{
		bool bAllLoaded = true;

		for (const TPair<EInterchangeMaterialXShaders, FSoftObjectPath>& Pair : ObjectPaths)
		{
			const FSoftObjectPath& ObjectPath = Pair.Get<1>();

			if (!ObjectPath.ResolveObject())
			{
				FString PackagePath = ObjectPath.GetLongPackageName();
				if (FPackageName::DoesPackageExist(PackagePath))
				{
					UObject* Asset = ObjectPath.TryLoad();
					if (!Asset)
					{
						UE_LOG(LogInterchangePipeline, Warning, TEXT("Couldn't load %s"), *PackagePath);
						bAllLoaded = false;
					}
#if WITH_EDITOR
					else
					{
						if (Pair.Get<EInterchangeMaterialXShaders>() == EInterchangeMaterialXShaders::StandardSurface)
						{
							bAllLoaded = !ShouldFilterAssets(Cast<UMaterialFunction>(Asset), StandardSurfaceInputs, StandardSurfaceOutputs);
						}
						else if (Pair.Get<EInterchangeMaterialXShaders>() == EInterchangeMaterialXShaders::StandardSurfaceTransmission)
						{
							bAllLoaded = !ShouldFilterAssets(Cast<UMaterialFunction>(Asset), TransmissionSurfaceInputs, TransmissionSurfaceOutputs);
						}
						else if (Pair.Get<EInterchangeMaterialXShaders>() == EInterchangeMaterialXShaders::SurfaceUnlit)
						{
							bAllLoaded = !ShouldFilterAssets(Cast<UMaterialFunction>(Asset), SurfaceUnlitInputs, SurfaceUnlitOutputs);
						}
						else if (Pair.Get<EInterchangeMaterialXShaders>() == EInterchangeMaterialXShaders::UsdPreviewSurface)
						{
							bAllLoaded = !ShouldFilterAssets(Cast<UMaterialFunction>(Asset), UsdPreviewSurfaceInputs, UsdPreviewSurfaceOutputs);
						}
					}
#endif // WITH_EDITOR
				}
				else
				{
					UE_LOG(LogInterchangePipeline, Warning, TEXT("Couldn't find %s"), *PackagePath);
					bAllLoaded = false;
				}
			}
		}

		return bAllLoaded;
	};

	return ArePackagesLoaded(PredefinedSurfaceShaders);
}

FString UMaterialXPipelineSettings::GetAssetPathString(EInterchangeMaterialXShaders ShaderType) const
{
	if (const FSoftObjectPath* ObjectPath = PredefinedSurfaceShaders.Find(ShaderType))
	{
		return ObjectPath->GetAssetPathString();
	}

	return {};
}

#if WITH_EDITOR
TSet<FName> UMaterialXPipelineSettings::StandardSurfaceInputs
{
	UE::Interchange::Materials::StandardSurface::Parameters::Base,
	UE::Interchange::Materials::StandardSurface::Parameters::BaseColor,
	UE::Interchange::Materials::StandardSurface::Parameters::DiffuseRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::Metalness,
	UE::Interchange::Materials::StandardSurface::Parameters::Specular,
	UE::Interchange::Materials::StandardSurface::Parameters::SpecularRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::SpecularIOR,
	UE::Interchange::Materials::StandardSurface::Parameters::SpecularAnisotropy,
	UE::Interchange::Materials::StandardSurface::Parameters::SpecularRotation,
	UE::Interchange::Materials::StandardSurface::Parameters::Subsurface,
	UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceColor,
	UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceRadius,
	UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceScale,
	UE::Interchange::Materials::StandardSurface::Parameters::Sheen,
	UE::Interchange::Materials::StandardSurface::Parameters::SheenColor,
	UE::Interchange::Materials::StandardSurface::Parameters::SheenRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::Coat,
	UE::Interchange::Materials::StandardSurface::Parameters::CoatColor,
	UE::Interchange::Materials::StandardSurface::Parameters::CoatRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::CoatNormal,
	UE::Interchange::Materials::StandardSurface::Parameters::ThinFilmThickness,
	UE::Interchange::Materials::StandardSurface::Parameters::Emission,
	UE::Interchange::Materials::StandardSurface::Parameters::EmissionColor,
	UE::Interchange::Materials::StandardSurface::Parameters::Normal,
	UE::Interchange::Materials::StandardSurface::Parameters::Tangent
};

TSet<FName> UMaterialXPipelineSettings::StandardSurfaceOutputs
{
	TEXT("Base Color"), // MX_StandardSurface has BaseColor with a whitespace, this should be fixed in further release
	UE::Interchange::Materials::PBRMR::Parameters::Metallic,
	UE::Interchange::Materials::PBRMR::Parameters::Specular,
	UE::Interchange::Materials::PBRMR::Parameters::Roughness,
	UE::Interchange::Materials::PBRMR::Parameters::Anisotropy,
	UE::Interchange::Materials::PBRMR::Parameters::EmissiveColor,
	UE::Interchange::Materials::PBRMR::Parameters::Opacity,
	UE::Interchange::Materials::PBRMR::Parameters::Normal,
	UE::Interchange::Materials::PBRMR::Parameters::Tangent,
	UE::Interchange::Materials::Sheen::Parameters::SheenRoughness,
	UE::Interchange::Materials::Sheen::Parameters::SheenColor,
	UE::Interchange::Materials::Subsurface::Parameters::SubsurfaceColor,
	UE::Interchange::Materials::ClearCoat::Parameters::ClearCoat,
	UE::Interchange::Materials::ClearCoat::Parameters::ClearCoatRoughness,
	UE::Interchange::Materials::ClearCoat::Parameters::ClearCoatNormal
};

TSet<FName> UMaterialXPipelineSettings::TransmissionSurfaceInputs
{
	UE::Interchange::Materials::StandardSurface::Parameters::Base,
	UE::Interchange::Materials::StandardSurface::Parameters::BaseColor,
	UE::Interchange::Materials::StandardSurface::Parameters::DiffuseRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::Metalness,
	UE::Interchange::Materials::StandardSurface::Parameters::Specular,
	UE::Interchange::Materials::StandardSurface::Parameters::SpecularRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::SpecularIOR,
	UE::Interchange::Materials::StandardSurface::Parameters::SpecularAnisotropy,
	UE::Interchange::Materials::StandardSurface::Parameters::SpecularRotation,
	UE::Interchange::Materials::StandardSurface::Parameters::Transmission,
	UE::Interchange::Materials::StandardSurface::Parameters::TransmissionColor,
	UE::Interchange::Materials::StandardSurface::Parameters::TransmissionDepth,
	UE::Interchange::Materials::StandardSurface::Parameters::TransmissionScatter,
	UE::Interchange::Materials::StandardSurface::Parameters::TransmissionScatterAnisotropy,
	UE::Interchange::Materials::StandardSurface::Parameters::TransmissionDispersion,
	UE::Interchange::Materials::StandardSurface::Parameters::TransmissionExtraRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::Subsurface,
	UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceColor,
	UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceRadius,
	UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceScale,
	UE::Interchange::Materials::StandardSurface::Parameters::Sheen,
	UE::Interchange::Materials::StandardSurface::Parameters::SheenColor,
	UE::Interchange::Materials::StandardSurface::Parameters::SheenRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::Coat,
	UE::Interchange::Materials::StandardSurface::Parameters::CoatColor,
	UE::Interchange::Materials::StandardSurface::Parameters::CoatRoughness,
	UE::Interchange::Materials::StandardSurface::Parameters::CoatNormal,
	UE::Interchange::Materials::StandardSurface::Parameters::ThinFilmThickness,
	UE::Interchange::Materials::StandardSurface::Parameters::Emission,
	UE::Interchange::Materials::StandardSurface::Parameters::EmissionColor,
	UE::Interchange::Materials::StandardSurface::Parameters::Normal,
};

TSet<FName> UMaterialXPipelineSettings::TransmissionSurfaceOutputs
{
	UE::Interchange::Materials::PBRMR::Parameters::BaseColor,
	UE::Interchange::Materials::PBRMR::Parameters::Metallic,
	UE::Interchange::Materials::PBRMR::Parameters::Specular,
	UE::Interchange::Materials::PBRMR::Parameters::Roughness,
	UE::Interchange::Materials::PBRMR::Parameters::Anisotropy,
	UE::Interchange::Materials::PBRMR::Parameters::EmissiveColor,
	UE::Interchange::Materials::PBRMR::Parameters::Opacity,
	UE::Interchange::Materials::PBRMR::Parameters::Normal,
	UE::Interchange::Materials::PBRMR::Parameters::Tangent,
	UE::Interchange::Materials::PBRMR::Parameters::Refraction,
	UE::Interchange::Materials::ThinTranslucent::Parameters::TransmissionColor
};

TSet<FName> UMaterialXPipelineSettings::SurfaceUnlitInputs
{
	UE::Interchange::Materials::SurfaceUnlit::Parameters::Emission,
	UE::Interchange::Materials::SurfaceUnlit::Parameters::EmissionColor,
	UE::Interchange::Materials::SurfaceUnlit::Parameters::Transmission,
	UE::Interchange::Materials::SurfaceUnlit::Parameters::TransmissionColor,
	UE::Interchange::Materials::SurfaceUnlit::Parameters::Opacity
};

TSet<FName> UMaterialXPipelineSettings::SurfaceUnlitOutputs
{
	UE::Interchange::Materials::Common::Parameters::EmissiveColor,
	UE::Interchange::Materials::Common::Parameters::Opacity,
	TEXT("OpacityMask")
};

TSet<FName> UMaterialXPipelineSettings::UsdPreviewSurfaceInputs
{
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::DiffuseColor,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::EmissiveColor,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::SpecularColor,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::Metallic,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::Roughness,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::Clearcoat,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::ClearcoatRoughness,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::Opacity,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::OpacityThreshold,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::IOR,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::Normal,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::Displacement,
	UE::Interchange::Materials::UsdPreviewSurface::Parameters::Occlusion
};

TSet<FName> UMaterialXPipelineSettings::UsdPreviewSurfaceOutputs
{
	UE::Interchange::Materials::PBRMR::Parameters::BaseColor,
	UE::Interchange::Materials::PBRMR::Parameters::Metallic,
	UE::Interchange::Materials::PBRMR::Parameters::Specular,
	UE::Interchange::Materials::PBRMR::Parameters::Roughness,
	UE::Interchange::Materials::PBRMR::Parameters::EmissiveColor,
	UE::Interchange::Materials::PBRMR::Parameters::Opacity,
	UE::Interchange::Materials::PBRMR::Parameters::Normal,
	UE::Interchange::Materials::Common::Parameters::Refraction,
	UE::Interchange::Materials::Common::Parameters::Occlusion,
	UE::Interchange::Materials::ClearCoat::Parameters::ClearCoat,
	UE::Interchange::Materials::ClearCoat::Parameters::ClearCoatRoughness,
};

bool UMaterialXPipelineSettings::ShouldFilterAssets(UMaterialFunction* Asset, const TSet<FName>& Inputs, const TSet<FName>& Outputs)
{
	int32 InputMatches = 0;
	int32 OutputMatches = 0;

	if (Asset != nullptr)
	{
		TArray<FFunctionExpressionInput> ExpressionInputs;
		TArray<FFunctionExpressionOutput> ExpressionOutputs;
		Asset->GetInputsAndOutputs(ExpressionInputs, ExpressionOutputs);

		for (const FFunctionExpressionInput& ExpressionInput : ExpressionInputs)
		{
			if (Inputs.Find(ExpressionInput.Input.InputName))
			{
				InputMatches++;
			}
		}

		for (const FFunctionExpressionOutput& ExpressionOutput : ExpressionOutputs)
		{
			if (Outputs.Find(ExpressionOutput.Output.OutputName))
			{
				OutputMatches++;
			}
		}
	}

	// we allow at least one input of the same name, but we should have exactly the same outputs
	return !(InputMatches > 0 && OutputMatches == Outputs.Num());
}
#endif // WITH_EDITOR

