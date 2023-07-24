// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/GLTFDelayedMaterialTasks.h"
#include "Utilities/GLTFCoreUtilities.h"
#include "Converters/GLTFNameUtilities.h"
#include "Converters/GLTFMaterialUtilities.h"
#include "Builders/GLTFContainerBuilder.h"
#include "Utilities/GLTFProxyMaterialUtilities.h"
#include "MaterialDomain.h"
#include "Materials/GLTFProxyMaterialInfo.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"

namespace
{
	// Component masks
	const FLinearColor RedMask(1.0f, 0.0f, 0.0f, 0.0f);
	const FLinearColor GreenMask(0.0f, 1.0f, 0.0f, 0.0f);
	const FLinearColor BlueMask(0.0f, 0.0f, 1.0f, 0.0f);
	const FLinearColor AlphaMask(0.0f, 0.0f, 0.0f, 1.0f);
	const FLinearColor RgbMask = RedMask + GreenMask + BlueMask;
	const FLinearColor RgbaMask = RgbMask + AlphaMask;

	// Property-specific component masks
	const FLinearColor BaseColorMask = RgbMask;
	const FLinearColor OpacityMask = AlphaMask;
	const FLinearColor MetallicMask = BlueMask;
	const FLinearColor RoughnessMask = GreenMask;
	const FLinearColor OcclusionMask = RedMask;
	const FLinearColor ClearCoatMask = RedMask;
	const FLinearColor ClearCoatRoughnessMask = GreenMask;

	// Ideal masks for texture-inputs (doesn't require baking)
	const TArray<FLinearColor> DefaultColorInputMasks = { RgbMask, RgbaMask };
	const TArray<FLinearColor> BaseColorInputMasks = { BaseColorMask };
	const TArray<FLinearColor> OpacityInputMasks = { OpacityMask };
	const TArray<FLinearColor> MetallicInputMasks = { MetallicMask };
	const TArray<FLinearColor> RoughnessInputMasks = { RoughnessMask };
	const TArray<FLinearColor> OcclusionInputMasks = { OcclusionMask };
	const TArray<FLinearColor> ClearCoatInputMasks = { ClearCoatMask };
	const TArray<FLinearColor> ClearCoatRoughnessInputMasks = { ClearCoatRoughnessMask };
}

void FGLTFDelayedMaterialTask::Process()
{
	const UMaterial* BaseMaterial = Material->GetMaterial();
	if (BaseMaterial->MaterialDomain != MD_Surface)
	{
		// TODO: report warning (non-surface materials not supported, will be treated as surface)
	}

	JsonMaterial->Name = GetMaterialName();
	JsonMaterial->AlphaCutoff = Material->GetOpacityMaskClipValue();
	JsonMaterial->DoubleSided = Material->IsTwoSided();

	ConvertShadingModel(JsonMaterial->ShadingModel);
	ConvertAlphaMode(JsonMaterial->AlphaMode);

	if (FGLTFProxyMaterialUtilities::IsProxyMaterial(BaseMaterial))
	{
		GetProxyParameters(*JsonMaterial);
		return;
	}

#if WITH_EDITOR
	if (JsonMaterial->ShadingModel != EGLTFJsonShadingModel::None)
	{
		const FMaterialPropertyEx BaseColorProperty = JsonMaterial->ShadingModel == EGLTFJsonShadingModel::Unlit ? MP_EmissiveColor : MP_BaseColor;
		const FMaterialPropertyEx OpacityProperty = JsonMaterial->AlphaMode == EGLTFJsonAlphaMode::Mask ? MP_OpacityMask : MP_Opacity;

		// TODO: check if a property is active before trying to get it (i.e. Material->IsPropertyActive)

		if (JsonMaterial->AlphaMode == EGLTFJsonAlphaMode::Opaque)
		{
			if (!TryGetConstantColor(JsonMaterial->PBRMetallicRoughness.BaseColorFactor, BaseColorProperty))
			{
				if (!TryGetSourceTexture(JsonMaterial->PBRMetallicRoughness.BaseColorTexture, BaseColorProperty, DefaultColorInputMasks))
				{
					if (!TryGetBakedMaterialProperty(JsonMaterial->PBRMetallicRoughness.BaseColorTexture, JsonMaterial->PBRMetallicRoughness.BaseColorFactor, BaseColorProperty, TEXT("BaseColor")))
					{
						Builder.LogWarning(FString::Printf(TEXT("Failed to export %s for material %s"), *BaseColorProperty.ToString(), *Material->GetName()));
					}
				}
			}

			JsonMaterial->PBRMetallicRoughness.BaseColorFactor.A = 1.0f; // make sure base color is opaque
		}
		else
		{
			if (!TryGetBaseColorAndOpacity(JsonMaterial->PBRMetallicRoughness, BaseColorProperty, OpacityProperty))
			{
				Builder.LogWarning(FString::Printf(TEXT("Failed to export %s and %s for material %s"), *BaseColorProperty.ToString(), *OpacityProperty.ToString(), *Material->GetName()));
			}
		}

		if (JsonMaterial->ShadingModel == EGLTFJsonShadingModel::Default || JsonMaterial->ShadingModel == EGLTFJsonShadingModel::ClearCoat)
		{
			const FMaterialPropertyEx MetallicProperty = MP_Metallic;
			const FMaterialPropertyEx RoughnessProperty = MP_Roughness;

			if (!TryGetMetallicAndRoughness(JsonMaterial->PBRMetallicRoughness, MetallicProperty, RoughnessProperty))
			{
				Builder.LogWarning(FString::Printf(TEXT("Failed to export %s and %s for material %s"), *MetallicProperty.ToString(), *RoughnessProperty.ToString(), *Material->GetName()));
			}

			const FMaterialPropertyEx EmissiveProperty = MP_EmissiveColor;
			if (!TryGetEmissive(*JsonMaterial, EmissiveProperty))
			{
				Builder.LogWarning(FString::Printf(TEXT("Failed to export %s for material %s"), *EmissiveProperty.ToString(), *Material->GetName()));
			}

			const FMaterialPropertyEx NormalProperty = JsonMaterial->ShadingModel == EGLTFJsonShadingModel::ClearCoat ? FMaterialPropertyEx::ClearCoatBottomNormal : FMaterialPropertyEx(MP_Normal);
			if (IsPropertyNonDefault(NormalProperty))
			{
				if (!TryGetSourceTexture(JsonMaterial->NormalTexture, NormalProperty, DefaultColorInputMasks))
				{
					if (!TryGetBakedMaterialProperty(JsonMaterial->NormalTexture, NormalProperty, TEXT("Normal")))
					{
						Builder.LogWarning(FString::Printf(TEXT("Failed to export %s for material %s"), *NormalProperty.ToString(), *Material->GetName()));
					}
				}
			}

			const FMaterialPropertyEx AmbientOcclusionProperty = MP_AmbientOcclusion;
			if (IsPropertyNonDefault(AmbientOcclusionProperty))
			{
				if (!TryGetSourceTexture(JsonMaterial->OcclusionTexture, AmbientOcclusionProperty, OcclusionInputMasks))
				{
					if (!TryGetBakedMaterialProperty(JsonMaterial->OcclusionTexture, AmbientOcclusionProperty, TEXT("Occlusion")))
					{
						Builder.LogWarning(FString::Printf(TEXT("Failed to export %s for material %s"), *AmbientOcclusionProperty.ToString(), *Material->GetName()));
					}
				}
			}

			if (JsonMaterial->ShadingModel == EGLTFJsonShadingModel::ClearCoat)
			{
				const FMaterialPropertyEx ClearCoatProperty = MP_CustomData0;
				const FMaterialPropertyEx ClearCoatRoughnessProperty = MP_CustomData1;

				if (!TryGetClearCoatRoughness(JsonMaterial->ClearCoat, ClearCoatProperty, ClearCoatRoughnessProperty))
				{
					Builder.LogWarning(FString::Printf(TEXT("Failed to export %s and %s for material %s"), *ClearCoatProperty.ToString(), *ClearCoatRoughnessProperty.ToString(), *Material->GetName()));
				}

				const FMaterialPropertyEx ClearCoatNormalProperty = MP_Normal;
				if (IsPropertyNonDefault(ClearCoatNormalProperty))
				{
					if (!TryGetSourceTexture(JsonMaterial->ClearCoat.ClearCoatNormalTexture, ClearCoatNormalProperty, DefaultColorInputMasks))
					{
						if (!TryGetBakedMaterialProperty(JsonMaterial->ClearCoat.ClearCoatNormalTexture, ClearCoatNormalProperty, TEXT("ClearCoatNormal")))
						{
							Builder.LogWarning(FString::Printf(TEXT("Failed to export %s for material %s"), *ClearCoatNormalProperty.ToString(), *Material->GetName()));
						}
					}
				}
			}
		}
	}

	// Warn if properties baked using mesh data are using overlapping UVs
	if (MeshData != nullptr && MeshDataBakedProperties.Num() > 0)
	{
		const FGLTFMeshData* ParentMeshData = MeshData->GetParent();

		const float UVOverlapThreshold = 1.0f / 100.0f;
		const float UVOverlap = UVOverlapChecker.GetOrAdd(&ParentMeshData->Description, SectionIndices, MeshData->BakeUsingTexCoord);

		if (UVOverlap > UVOverlapThreshold)
		{
			FString SectionString = TEXT("mesh section");
			SectionString += SectionIndices.Num() > 1 ? TEXT("s ") : TEXT(" ");
			SectionString += FString::JoinBy(SectionIndices, TEXT(", "), FString::FromInt);

			Builder.LogWarning(FString::Printf(
				TEXT("Material %s is baked using mesh data from %s but the lightmap UV (channel %d) are overlapping by %.2f%% (in %s) and may produce incorrect results"),
				*Material->GetName(),
				*ParentMeshData->Name,
				MeshData->BakeUsingTexCoord,
				UVOverlap * 100,
				*SectionString));
		}
	}
#else
	Builder.LogError(FString::Printf(
		TEXT("Can't properly export material %s in a runtime environment without a glTF proxy. In the Unreal Editor's Content Browser, please right-click on the material, then select Create glTF Proxy Material"),
		*Material->GetName()));
#endif
}

FString FGLTFDelayedMaterialTask::GetMaterialName() const
{
	FString MaterialName = Material->GetName();

	if (MeshData != nullptr)
	{
		MaterialName += TEXT("_") + MeshData->Name;
	}

	return MaterialName;
}

FString FGLTFDelayedMaterialTask::GetBakedTextureName(const FString& PropertyName) const
{
	return GetMaterialName() + TEXT("_") + PropertyName;
}

void FGLTFDelayedMaterialTask::GetProxyParameters(FGLTFJsonMaterial& OutMaterial) const
{
	GetProxyParameter(FGLTFProxyMaterialInfo::BaseColorFactor, OutMaterial.PBRMetallicRoughness.BaseColorFactor);
	GetProxyParameter(FGLTFProxyMaterialInfo::BaseColor, OutMaterial.PBRMetallicRoughness.BaseColorTexture);

	if (OutMaterial.ShadingModel == EGLTFJsonShadingModel::Default || OutMaterial.ShadingModel == EGLTFJsonShadingModel::ClearCoat)
	{
		GetProxyParameter(FGLTFProxyMaterialInfo::EmissiveFactor, OutMaterial.EmissiveFactor);
		GetProxyParameter(FGLTFProxyMaterialInfo::Emissive, OutMaterial.EmissiveTexture);

		GetProxyParameter(FGLTFProxyMaterialInfo::MetallicFactor, OutMaterial.PBRMetallicRoughness.MetallicFactor);
		GetProxyParameter(FGLTFProxyMaterialInfo::RoughnessFactor, OutMaterial.PBRMetallicRoughness.RoughnessFactor);
		GetProxyParameter(FGLTFProxyMaterialInfo::MetallicRoughness, OutMaterial.PBRMetallicRoughness.MetallicRoughnessTexture);

		if (HasProxyParameter(FGLTFProxyMaterialInfo::Normal.Texture))
		{
			if (OutMaterial.ShadingModel == EGLTFJsonShadingModel::ClearCoat && !FGLTFMaterialUtilities::IsClearCoatBottomNormalEnabled())
			{
				Builder.LogWarning(FString::Printf(
					TEXT("Proxy material %s won't be exported with (bottom) normal because ClearCoatEnableSecondNormal in project rendering settings is disabled"),
					*Material->GetName()));
			}
			else
			{
				GetProxyParameter(FGLTFProxyMaterialInfo::NormalScale, OutMaterial.NormalTexture.Scale);
				GetProxyParameter(FGLTFProxyMaterialInfo::Normal, OutMaterial.NormalTexture);
			}
		}

		GetProxyParameter(FGLTFProxyMaterialInfo::OcclusionStrength, OutMaterial.OcclusionTexture.Strength);
		GetProxyParameter(FGLTFProxyMaterialInfo::Occlusion, OutMaterial.OcclusionTexture);

		if (OutMaterial.ShadingModel == EGLTFJsonShadingModel::ClearCoat)
		{
			GetProxyParameter(FGLTFProxyMaterialInfo::ClearCoatFactor, OutMaterial.ClearCoat.ClearCoatFactor);
			GetProxyParameter(FGLTFProxyMaterialInfo::ClearCoat, OutMaterial.ClearCoat.ClearCoatTexture);

			GetProxyParameter(FGLTFProxyMaterialInfo::ClearCoatRoughnessFactor, OutMaterial.ClearCoat.ClearCoatRoughnessFactor);
			GetProxyParameter(FGLTFProxyMaterialInfo::ClearCoatRoughness, OutMaterial.ClearCoat.ClearCoatRoughnessTexture);

			GetProxyParameter(FGLTFProxyMaterialInfo::ClearCoatNormalScale, OutMaterial.ClearCoat.ClearCoatNormalTexture.Scale);
			GetProxyParameter(FGLTFProxyMaterialInfo::ClearCoatNormal, OutMaterial.ClearCoat.ClearCoatNormalTexture);
		}
	}
}

void FGLTFDelayedMaterialTask::GetProxyParameter(const TGLTFProxyMaterialParameterInfo<float>& ParameterInfo, float& OutValue) const
{
	ParameterInfo.Get(Material, OutValue, true);
}

void FGLTFDelayedMaterialTask::GetProxyParameter(const TGLTFProxyMaterialParameterInfo<FLinearColor>& ParameterInfo, FGLTFJsonColor3& OutValue) const
{
	FLinearColor Value;
	if (ParameterInfo.Get(Material, Value, true))
	{
		OutValue = FGLTFCoreUtilities::ConvertColor3(Value);
	}
}

void FGLTFDelayedMaterialTask::GetProxyParameter(const TGLTFProxyMaterialParameterInfo<FLinearColor>& ParameterInfo, FGLTFJsonColor4& OutValue) const
{
	FLinearColor Value;
	if (ParameterInfo.Get(Material, Value, true))
	{
		OutValue = FGLTFCoreUtilities::ConvertColor(Value);
	}
}

void FGLTFDelayedMaterialTask::GetProxyParameter(const FGLTFProxyMaterialTextureParameterInfo& ParameterInfo, FGLTFJsonTextureInfo& OutValue) const
{
	UTexture* Texture;
	if (!ParameterInfo.Texture.Get(Material, Texture, true) || Texture == nullptr)
	{
		return;
	}

	const bool bSRGB = ParameterInfo == FGLTFProxyMaterialInfo::BaseColor || ParameterInfo == FGLTFProxyMaterialInfo::Emissive;
	OutValue.Index = Builder.AddUniqueTexture(Texture, bSRGB);

	float UVIndex;
	if (ParameterInfo.UVIndex.Get(Material, UVIndex, true))
	{
		OutValue.TexCoord = FMath::RoundToInt(UVIndex);
	}

	FLinearColor UVOffset;
	if (ParameterInfo.UVOffset.Get(Material, UVOffset, true))
	{
		OutValue.Transform.Offset.X = UVOffset.R;
		OutValue.Transform.Offset.Y = UVOffset.G;
	}

	FLinearColor UVScale;
	if (ParameterInfo.UVScale.Get(Material, UVScale, true))
	{
		OutValue.Transform.Scale.X = UVScale.R;
		OutValue.Transform.Scale.Y = UVScale.G;
	}

	float UVRotation;
	if (ParameterInfo.UVRotation.Get(Material, UVRotation, true))
	{
		OutValue.Transform.Rotation = UVRotation;
	}

	if (!Builder.ExportOptions->bExportTextureTransforms && !OutValue.Transform.IsExactlyDefault())
	{
		Builder.LogWarning(FString::Printf(
			TEXT("Texture coordinates [%d] in %s for material %s are transformed, but texture transform is disabled by export options"),
			OutValue.TexCoord,
			*ParameterInfo.Texture.ToString(),
			*Material->GetName()));
		OutValue.Transform = {};
	}
}

EMaterialShadingModel FGLTFDelayedMaterialTask::GetShadingModel() const
{
	const FMaterialShadingModelField Possibilities = Material->GetShadingModels();
	const int32 PossibilitiesCount = Possibilities.CountShadingModels();

	if (PossibilitiesCount == 0)
	{
		const EMaterialShadingModel ShadingModel = MSM_DefaultLit;
		Builder.LogWarning(FString::Printf(
			TEXT("No shading model defined for material %s, will export as %s"),
			*Material->GetName(),
			*FGLTFNameUtilities::GetName(ShadingModel)));
		return ShadingModel;
	}

	if (PossibilitiesCount > 1)
	{
#if WITH_EDITOR
		if (!FApp::CanEverRender())
		{
			const EMaterialShadingModel ShadingModel = FGLTFMaterialUtilities::GetRichestShadingModel(Possibilities);
			Builder.LogWarning(FString::Printf(
				TEXT("Can't evaluate shading model expression in material %s because renderer missing, will export as %s"),
				*Material->GetName(),
				*FGLTFNameUtilities::GetName(ShadingModel)));
			return ShadingModel;
		}

		if (Material->IsShadingModelFromMaterialExpression())
		{
			const FMaterialShadingModelField Evaluation = FGLTFMaterialUtilities::EvaluateShadingModelExpression(Material);
			const int32 EvaluationCount = Evaluation.CountShadingModels();

			if (EvaluationCount == 0)
			{
				const EMaterialShadingModel ShadingModel = FGLTFMaterialUtilities::GetRichestShadingModel(Possibilities);
				Builder.LogWarning(FString::Printf(
					TEXT("Evaluation of shading model expression in material %s returned none, will export as %s"),
					*Material->GetName(),
					*FGLTFNameUtilities::GetName(ShadingModel)));
				return ShadingModel;
			}

			if (EvaluationCount > 1)
			{
				const EMaterialShadingModel ShadingModel = FGLTFMaterialUtilities::GetRichestShadingModel(Evaluation);
				Builder.LogWarning(FString::Printf(
					TEXT("Evaluation of shading model expression in material %s is inconclusive (%s), will export as %s"),
					*Material->GetName(),
					*FGLTFMaterialUtilities::ShadingModelsToString(Evaluation),
					*FGLTFNameUtilities::GetName(ShadingModel)));
				return ShadingModel;
			}

			return Evaluation.GetFirstShadingModel();
		}

		// we should never end up here
#else
		const EMaterialShadingModel ShadingModel = FGLTFMaterialUtilities::GetRichestShadingModel(Possibilities);
		Builder.LogWarning(FString::Printf(
			TEXT("Can't evaluate shading model expression in material %s without editor, will export as %s"),
			*Material->GetName(),
			*FGLTFNameUtilities::GetName(ShadingModel)));
		return ShadingModel;
#endif
	}

	return Possibilities.GetFirstShadingModel();
}

void FGLTFDelayedMaterialTask::ConvertShadingModel(EGLTFJsonShadingModel& OutShadingModel) const
{
	EMaterialShadingModel ShadingModel = GetShadingModel();

	const EBlendMode BlendMode = Material->GetBlendMode();
	if (ShadingModel == MSM_ClearCoat && BlendMode != BLEND_Opaque && BlendMode != BLEND_Masked)
	{
		// NOTE: Unreal seems to disable clear coat when blend mode anything but opaque or masked
		ShadingModel = MSM_DefaultLit;
	}

	OutShadingModel = FGLTFCoreUtilities::ConvertShadingModel(ShadingModel);
	if (OutShadingModel == EGLTFJsonShadingModel::None)
	{
		OutShadingModel = EGLTFJsonShadingModel::Default;

		Builder.LogWarning(FString::Printf(
			TEXT("Unsupported shading model (%s) in material %s, will export as %s"),
			*FGLTFNameUtilities::GetName(ShadingModel),
			*Material->GetName(),
			*FGLTFNameUtilities::GetName(MSM_DefaultLit)));
		return;
	}

	if (OutShadingModel == EGLTFJsonShadingModel::Unlit && !Builder.ExportOptions->bExportUnlitMaterials)
	{
		OutShadingModel = EGLTFJsonShadingModel::Default;

		Builder.LogWarning(FString::Printf(
			TEXT("Shading model (%s) in material %s disabled by export options, will export as %s"),
			*FGLTFNameUtilities::GetName(ShadingModel),
			*Material->GetName(),
			*FGLTFNameUtilities::GetName(MSM_DefaultLit)));
		return;
	}

	if (OutShadingModel == EGLTFJsonShadingModel::ClearCoat && !Builder.ExportOptions->bExportClearCoatMaterials)
	{
		OutShadingModel = EGLTFJsonShadingModel::Default;

		Builder.LogWarning(FString::Printf(
			TEXT("Shading model (%s) in material %s disabled by export options, will export as %s"),
			*FGLTFNameUtilities::GetName(ShadingModel),
			*Material->GetName(),
			*FGLTFNameUtilities::GetName(MSM_DefaultLit)));
	}
}

void FGLTFDelayedMaterialTask::ConvertAlphaMode(EGLTFJsonAlphaMode& OutAlphaMode) const
{
	const EBlendMode BlendMode = Material->GetBlendMode();

	OutAlphaMode = FGLTFCoreUtilities::ConvertAlphaMode(BlendMode);
	if (OutAlphaMode == EGLTFJsonAlphaMode::None)
	{
		OutAlphaMode = EGLTFJsonAlphaMode::Blend;

		Builder.LogWarning(FString::Printf(
			TEXT("Unsupported blend mode (%s) in material %s, will export as %s"),
			*FGLTFNameUtilities::GetName(BlendMode),
			*Material->GetName(),
			*FGLTFNameUtilities::GetName(BLEND_Translucent)));
		return;
	}
}

#if WITH_EDITOR

bool FGLTFDelayedMaterialTask::TryGetBaseColorAndOpacity(FGLTFJsonPBRMetallicRoughness& OutPBRParams, const FMaterialPropertyEx& BaseColorProperty, const FMaterialPropertyEx& OpacityProperty)
{
	const bool bIsBaseColorConstant = TryGetConstantColor(OutPBRParams.BaseColorFactor, BaseColorProperty);
	const bool bIsOpacityConstant = TryGetConstantScalar(OutPBRParams.BaseColorFactor.A, OpacityProperty);

	if (bIsBaseColorConstant && bIsOpacityConstant)
	{
		return true;
	}

	// NOTE: since we always bake the properties (for now) when at least property is non-const, we need
	// to reset the constant factors to their defaults. Otherwise the baked value of a constant property
	// would be scaled with the factor, i.e a double scaling.
	OutPBRParams.BaseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };

	const UTexture2D* BaseColorTexture;
	const UTexture2D* OpacityTexture;
	int32 BaseColorTexCoord;
	int32 OpacityTexCoord;
	FGLTFJsonTextureTransform BaseColorTransform;
	FGLTFJsonTextureTransform OpacityTransform;

	const bool bHasBaseColorSourceTexture = TryGetSourceTexture(BaseColorTexture, BaseColorTexCoord, BaseColorTransform, BaseColorProperty, BaseColorInputMasks);
	const bool bHasOpacitySourceTexture = TryGetSourceTexture(OpacityTexture, OpacityTexCoord, OpacityTransform, OpacityProperty, OpacityInputMasks);

	// Detect the "happy path" where both inputs share the same texture and are correctly masked.
	if (bHasBaseColorSourceTexture &&
		bHasOpacitySourceTexture &&
		BaseColorTexture == OpacityTexture &&
		BaseColorTexCoord == OpacityTexCoord &&
		BaseColorTransform.IsExactlyEqual(OpacityTransform))
	{
		OutPBRParams.BaseColorTexture.Index = Builder.AddUniqueTexture(BaseColorTexture, true);
		OutPBRParams.BaseColorTexture.TexCoord = BaseColorTexCoord;
		OutPBRParams.BaseColorTexture.Transform = BaseColorTransform;
		return true;
	}

	if (Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::Disabled)
	{
		Builder.LogWarning(FString::Printf(
			TEXT("%s and %s for material %s needs to bake, but material baking is disabled by export options"),
			*BaseColorProperty.ToString(),
			*OpacityProperty.ToString(),
			*Material->GetName()));
		return false;
	}

	const FIntPoint TextureSize = Builder.GetBakeSizeForMaterialProperty(Material, GetPropertyGroup(BaseColorProperty));
	const TextureAddress TextureAddress = Builder.GetBakeTilingForMaterialProperty(Material, GetPropertyGroup(BaseColorProperty));
	const TextureFilter TextureFilter = Builder.GetBakeFilterForMaterialProperty(Material, GetPropertyGroup(BaseColorProperty));

	const FGLTFPropertyBakeOutput BaseColorBakeOutput = BakeMaterialProperty(BaseColorProperty, BaseColorTexCoord, TextureSize, false);
	const FGLTFPropertyBakeOutput OpacityBakeOutput = BakeMaterialProperty(OpacityProperty, OpacityTexCoord, TextureSize, false);
	const float BaseColorScale = BaseColorProperty == MP_EmissiveColor ? BaseColorBakeOutput.EmissiveScale : 1;

	// Detect when both baked properties are constants, which means we can avoid exporting a texture
	if (BaseColorBakeOutput.bIsConstant && OpacityBakeOutput.bIsConstant)
	{
		FLinearColor BaseColorFactor(BaseColorBakeOutput.ConstantValue * BaseColorScale);
		BaseColorFactor.A = OpacityBakeOutput.ConstantValue.R;

		OutPBRParams.BaseColorFactor = FGLTFCoreUtilities::ConvertColor(BaseColorFactor);
		return true;
	}

	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		return true;
	}

	int32 CombinedTexCoord;

	// If one is constant, it means we can use the other's texture coords
	if (BaseColorBakeOutput.bIsConstant || BaseColorTexCoord == OpacityTexCoord)
	{
		CombinedTexCoord = OpacityTexCoord;
	}
	else if (OpacityBakeOutput.bIsConstant)
	{
		CombinedTexCoord = BaseColorTexCoord;
	}
	else
	{
		// TODO: report error (texture coordinate conflict)
		return false;
	}

	TGLTFSharedArray<FColor> CombinedPixels;
	CombinePixels(
		*BaseColorBakeOutput.Pixels,
		*OpacityBakeOutput.Pixels,
		*CombinedPixels,
		[](const FColor& BaseColor, const FColor& Opacity) -> FColor
		{
			return FColor(BaseColor.R, BaseColor.G, BaseColor.B, Opacity.R);
		});

	FGLTFJsonTexture* CombinedTexture = FGLTFMaterialUtilities::AddTexture(
		Builder,
		CombinedPixels,
		TextureSize,
		false,
		false,
		GetBakedTextureName(TEXT("BaseColor")),
		TextureAddress,
		TextureFilter);

	OutPBRParams.BaseColorTexture.TexCoord = CombinedTexCoord;
	OutPBRParams.BaseColorTexture.Index = CombinedTexture;

	// TODO: add support for KHR_materials_emissive_strength
	OutPBRParams.BaseColorFactor = FGLTFCoreUtilities::ConvertColor(FLinearColor(BaseColorScale, BaseColorScale, BaseColorScale));

	return true;
}

bool FGLTFDelayedMaterialTask::TryGetMetallicAndRoughness(FGLTFJsonPBRMetallicRoughness& OutPBRParams, const FMaterialPropertyEx& MetallicProperty, const FMaterialPropertyEx& RoughnessProperty)
{
	const bool bIsMetallicConstant = TryGetConstantScalar(OutPBRParams.MetallicFactor, MetallicProperty);
	const bool bIsRoughnessConstant = TryGetConstantScalar(OutPBRParams.RoughnessFactor, RoughnessProperty);

	if (bIsMetallicConstant && bIsRoughnessConstant)
	{
		return true;
	}

	// NOTE: since we always bake the properties (for now) when at least one property is non-const, we need
	// to reset the constant factors to their defaults. Otherwise the baked value of a constant property
	// would be scaled with the factor, i.e a double scaling.
	OutPBRParams.MetallicFactor = 1.0f;
	OutPBRParams.RoughnessFactor = 1.0f;

	const UTexture2D* MetallicTexture;
	const UTexture2D* RoughnessTexture;
	int32 MetallicTexCoord;
	int32 RoughnessTexCoord;
	FGLTFJsonTextureTransform MetallicTransform;
	FGLTFJsonTextureTransform RoughnessTransform;

	const bool bHasMetallicSourceTexture = TryGetSourceTexture(MetallicTexture, MetallicTexCoord, MetallicTransform, MetallicProperty, MetallicInputMasks);
	const bool bHasRoughnessSourceTexture = TryGetSourceTexture(RoughnessTexture, RoughnessTexCoord, RoughnessTransform, RoughnessProperty, RoughnessInputMasks);

	// Detect the "happy path" where both inputs share the same texture and are correctly masked.
	if (bHasMetallicSourceTexture &&
		bHasRoughnessSourceTexture &&
		MetallicTexture == RoughnessTexture &&
		MetallicTexCoord == RoughnessTexCoord &&
		MetallicTransform.IsExactlyEqual(RoughnessTransform))
	{
		OutPBRParams.MetallicRoughnessTexture.Index = Builder.AddUniqueTexture(MetallicTexture, false);
		OutPBRParams.MetallicRoughnessTexture.TexCoord = MetallicTexCoord;
		OutPBRParams.MetallicRoughnessTexture.Transform = MetallicTransform;
		return true;
	}

	if (Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::Disabled)
	{
		Builder.LogWarning(FString::Printf(
			TEXT("%s and %s for material %s needs to bake, but material baking is disabled by export options"),
			*MetallicProperty.ToString(),
			*RoughnessProperty.ToString(),
			*Material->GetName()));
		return false;
	}

	// TODO: add support for calculating the ideal resolution to use for baking based on connected (texture) nodes
	const FIntPoint TextureSize = Builder.GetBakeSizeForMaterialProperty(Material, GetPropertyGroup(MetallicProperty));
	const TextureAddress TextureAddress = Builder.GetBakeTilingForMaterialProperty(Material, GetPropertyGroup(MetallicProperty));
	const TextureFilter TextureFilter = Builder.GetBakeFilterForMaterialProperty(Material, GetPropertyGroup(MetallicProperty));

	FGLTFPropertyBakeOutput MetallicBakeOutput = BakeMaterialProperty(MetallicProperty, MetallicTexCoord, TextureSize, false);
	FGLTFPropertyBakeOutput RoughnessBakeOutput = BakeMaterialProperty(RoughnessProperty, RoughnessTexCoord, TextureSize, false);

	// Detect when both baked properties are constants, which means we can use factors and avoid exporting a texture
	if (MetallicBakeOutput.bIsConstant && RoughnessBakeOutput.bIsConstant)
	{
		OutPBRParams.MetallicFactor = MetallicBakeOutput.ConstantValue.R;
		OutPBRParams.RoughnessFactor = RoughnessBakeOutput.ConstantValue.R;
		return true;
	}

	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		return true;
	}

	int32 CombinedTexCoord;

	// If one is constant, it means we can use the other's texture coords
	if (MetallicBakeOutput.bIsConstant || MetallicTexCoord == RoughnessTexCoord)
	{
		CombinedTexCoord = RoughnessTexCoord;
	}
	else if (RoughnessBakeOutput.bIsConstant)
	{
		CombinedTexCoord = MetallicTexCoord;
	}
	else
	{
		// TODO: report error (texture coordinate conflict)
		return false;
	}

	TGLTFSharedArray<FColor> CombinedPixels;
	CombinePixels(
		*MetallicBakeOutput.Pixels,
		*RoughnessBakeOutput.Pixels,
		*CombinedPixels,
		[](const FColor& Metallic, const FColor& Roughness) -> FColor
		{
			return FColor(0, Roughness.R, Metallic.R);
		});

	FGLTFJsonTexture* CombinedTexture = FGLTFMaterialUtilities::AddTexture(
		Builder,
		CombinedPixels,
		TextureSize,
		true, // NOTE: we can ignore alpha in everything but TryGetBaseColorAndOpacity
		false,
		GetBakedTextureName(TEXT("MetallicRoughness")),
		TextureAddress,
		TextureFilter);

	OutPBRParams.MetallicRoughnessTexture.TexCoord = CombinedTexCoord;
	OutPBRParams.MetallicRoughnessTexture.Index = CombinedTexture;

	return true;
}

bool FGLTFDelayedMaterialTask::TryGetClearCoatRoughness(FGLTFJsonClearCoatExtension& OutExtParams, const FMaterialPropertyEx& IntensityProperty, const FMaterialPropertyEx& RoughnessProperty)
{
	const bool bIsIntensityConstant = TryGetConstantScalar(OutExtParams.ClearCoatFactor, IntensityProperty);
	const bool bIsRoughnessConstant = TryGetConstantScalar(OutExtParams.ClearCoatRoughnessFactor, RoughnessProperty);

	if (bIsIntensityConstant && bIsRoughnessConstant)
	{
		return true;
	}

	// NOTE: since we always bake the properties (for now) when at least one property is non-const, we need
	// to reset the constant factors to their defaults. Otherwise the baked value of a constant property
	// would be scaled with the factor, i.e a double scaling.
	OutExtParams.ClearCoatFactor = 1.0f;
	OutExtParams.ClearCoatRoughnessFactor = 1.0f;

	const UTexture2D* IntensityTexture;
	const UTexture2D* RoughnessTexture;
	int32 IntensityTexCoord;
	int32 RoughnessTexCoord;
	FGLTFJsonTextureTransform IntensityTransform;
	FGLTFJsonTextureTransform RoughnessTransform;

	const bool bHasIntensitySourceTexture = TryGetSourceTexture(IntensityTexture, IntensityTexCoord, IntensityTransform, IntensityProperty, ClearCoatInputMasks);
	const bool bHasRoughnessSourceTexture = TryGetSourceTexture(RoughnessTexture, RoughnessTexCoord, RoughnessTransform, RoughnessProperty, ClearCoatRoughnessInputMasks);

	// Detect the "happy path" where both inputs share the same texture and are correctly masked.
	if (bHasIntensitySourceTexture &&
		bHasRoughnessSourceTexture &&
		IntensityTexture == RoughnessTexture &&
		IntensityTexCoord == RoughnessTexCoord &&
		IntensityTransform.IsExactlyEqual(RoughnessTransform))
	{
		FGLTFJsonTexture* Texture = Builder.AddUniqueTexture(IntensityTexture, false);
		OutExtParams.ClearCoatTexture.Index = Texture;
		OutExtParams.ClearCoatTexture.TexCoord = IntensityTexCoord;
		OutExtParams.ClearCoatRoughnessTexture.Index = Texture;
		OutExtParams.ClearCoatRoughnessTexture.TexCoord = IntensityTexCoord;
		OutExtParams.ClearCoatRoughnessTexture.Transform = IntensityTransform;
		return true;
	}

	if (Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::Disabled)
	{
		Builder.LogWarning(FString::Printf(
			TEXT("%s and %s for material %s needs to bake, but material baking is disabled by export options"),
			*IntensityProperty.ToString(),
			*RoughnessProperty.ToString(),
			*Material->GetName()));
		return false;
	}

	const FIntPoint TextureSize = Builder.GetBakeSizeForMaterialProperty(Material, GetPropertyGroup(IntensityProperty));
	const TextureAddress TextureAddress = Builder.GetBakeTilingForMaterialProperty(Material,GetPropertyGroup(IntensityProperty));
	const TextureFilter TextureFilter = Builder.GetBakeFilterForMaterialProperty(Material, GetPropertyGroup(IntensityProperty));

	const FGLTFPropertyBakeOutput IntensityBakeOutput = BakeMaterialProperty(IntensityProperty, IntensityTexCoord, TextureSize, false);
	const FGLTFPropertyBakeOutput RoughnessBakeOutput = BakeMaterialProperty(RoughnessProperty, RoughnessTexCoord, TextureSize, false);

	// Detect when both baked properties are constants, which means we can use factors and avoid exporting a texture
	if (IntensityBakeOutput.bIsConstant && RoughnessBakeOutput.bIsConstant)
	{
		OutExtParams.ClearCoatFactor = IntensityBakeOutput.ConstantValue.R;
		OutExtParams.ClearCoatRoughnessFactor = RoughnessBakeOutput.ConstantValue.R;
		return true;
	}

	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		return true;
	}

	int32 CombinedTexCoord;

	// If one is constant, it means we can use the other's texture coords
	if (IntensityBakeOutput.bIsConstant || IntensityTexCoord == RoughnessTexCoord)
	{
		CombinedTexCoord = RoughnessTexCoord;
	}
	else if (RoughnessBakeOutput.bIsConstant)
	{
		CombinedTexCoord = IntensityTexCoord;
	}
	else
	{
		// TODO: report error (texture coordinate conflict)
		return false;
	}

	TGLTFSharedArray<FColor> CombinedPixels;
	CombinePixels(
		*IntensityBakeOutput.Pixels,
		*RoughnessBakeOutput.Pixels,
		*CombinedPixels,
		[](const FColor& Intensity, const FColor& Roughness) -> FColor
		{
			return FColor(Intensity.R, Roughness.R, 0);
		});

	FGLTFJsonTexture* CombinedTexture = FGLTFMaterialUtilities::AddTexture(
		Builder,
		CombinedPixels,
		TextureSize,
		true, // NOTE: we can ignore alpha in everything but TryGetBaseColorAndOpacity
		false,
		GetBakedTextureName(TEXT("ClearCoatRoughness")),
		TextureAddress,
		TextureFilter);

	OutExtParams.ClearCoatTexture.Index = CombinedTexture;
	OutExtParams.ClearCoatTexture.TexCoord = CombinedTexCoord;
	OutExtParams.ClearCoatRoughnessTexture.Index = CombinedTexture;
	OutExtParams.ClearCoatRoughnessTexture.TexCoord = CombinedTexCoord;

	return true;
}

bool FGLTFDelayedMaterialTask::TryGetEmissive(FGLTFJsonMaterial& OutMaterial, const FMaterialPropertyEx& EmissiveProperty)
{
	// TODO: right now we allow EmissiveFactor to be > 1.0 to support very bright emission, although it's not valid according to the glTF standard.
	// We may want to change this behaviour and store factors above 1.0 using a custom extension instead.

	if (TryGetConstantColor(OutMaterial.EmissiveFactor, MP_EmissiveColor))
	{
		return true;
	}

	if (TryGetSourceTexture(OutMaterial.EmissiveTexture, EmissiveProperty, DefaultColorInputMasks))
	{
		OutMaterial.EmissiveFactor = FGLTFJsonColor3::White;	// make sure texture is not multiplied with black
		return true;
	}

	if (Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::Disabled)
	{
		Builder.LogWarning(FString::Printf(
			TEXT("%s for material %s needs to bake, but material baking is disabled by export options"),
			*EmissiveProperty.ToString(),
			*Material->GetName()));
		return false;
	}

	FGLTFPropertyBakeOutput PropertyBakeOutput = BakeMaterialProperty(EmissiveProperty, OutMaterial.EmissiveTexture.TexCoord);
	const float EmissiveScale = PropertyBakeOutput.EmissiveScale;

	if (PropertyBakeOutput.bIsConstant)
	{
		const FLinearColor EmissiveColor = PropertyBakeOutput.ConstantValue;
		OutMaterial.EmissiveFactor = FGLTFCoreUtilities::ConvertColor3(EmissiveColor * EmissiveScale);
	}
	else
	{
		if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
		{
			OutMaterial.EmissiveTexture.Index = nullptr;
			return true;
		}

		if (!StoreBakedPropertyTexture(OutMaterial.EmissiveTexture, PropertyBakeOutput, TEXT("Emissive")))
		{
			return false;
		}

		OutMaterial.EmissiveFactor = { EmissiveScale, EmissiveScale, EmissiveScale };
	}

	return true;
}

bool FGLTFDelayedMaterialTask::IsPropertyNonDefault(const FMaterialPropertyEx& Property) const
{
	// Custom outputs are by definition standalone and never part of material attributes
	if (!Property.IsCustomOutput())
	{
		const bool bUseMaterialAttributes = Material->GetMaterial()->bUseMaterialAttributes;
		if (bUseMaterialAttributes)
		{
			// TODO: check if attribute property connected, i.e. Material->GetMaterial()->MaterialAttributes.IsConnected(Property)
			return true;
		}
	}

	const FExpressionInput* MaterialInput = FGLTFMaterialUtilities::GetInputForProperty(Material, Property);
	if (MaterialInput == nullptr)
	{
		return false;
	}

	const UMaterialExpression* Expression = MaterialInput->Expression;
	if (Expression == nullptr)
	{
		return false;
	}

	if (Property == FMaterialPropertyEx::ClearCoatBottomNormal && !FGLTFMaterialUtilities::IsClearCoatBottomNormalEnabled())
	{
		Builder.LogWarning(FString::Printf(
			TEXT("Material %s won't be exported with clear coat bottom normal because ClearCoatEnableSecondNormal in project rendering settings is disabled"),
			*Material->GetName()));
		return false;
	}

	return true;
}

bool FGLTFDelayedMaterialTask::TryGetConstantColor(FGLTFJsonColor3& OutValue, const FMaterialPropertyEx& Property) const
{
	FLinearColor Value;
	if (TryGetConstantColor(Value, Property))
	{
		OutValue = FGLTFCoreUtilities::ConvertColor3(Value);
		return true;
	}

	return false;
}

bool FGLTFDelayedMaterialTask::TryGetConstantColor(FGLTFJsonColor4& OutValue, const FMaterialPropertyEx& Property) const
{
	FLinearColor Value;
	if (TryGetConstantColor(Value, Property))
	{
		OutValue = FGLTFCoreUtilities::ConvertColor(Value);
		return true;
	}

	return false;
}

bool FGLTFDelayedMaterialTask::TryGetConstantColor(FLinearColor& OutValue, const FMaterialPropertyEx& Property) const
{
	const bool bUseMaterialAttributes = Material->GetMaterial()->bUseMaterialAttributes;
	if (bUseMaterialAttributes)
	{
		// TODO: check if attribute property connected, i.e. Material->GetMaterial()->MaterialAttributes.IsConnected(Property)
		return false;
	}

	const FMaterialInput<FColor>* MaterialInput = FGLTFMaterialUtilities::GetInputForProperty<FColor>(Material, Property);
	if (MaterialInput == nullptr)
	{
		// TODO: report error
		return false;
	}

	if (MaterialInput->UseConstant)
	{
		OutValue = { MaterialInput->Constant };
		return true;
	}

	const UMaterialExpression* Expression = MaterialInput->Expression;
	if (Expression == nullptr)
	{
		OutValue = FLinearColor(FGLTFMaterialUtilities::GetPropertyDefaultValue(Property));
		return true;
	}

	if (const UMaterialExpressionVectorParameter* VectorParameter = ExactCast<UMaterialExpressionVectorParameter>(Expression))
	{
		FLinearColor Value = VectorParameter->DefaultValue;

		const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		if (MaterialInstance != nullptr)
		{
			const FHashedMaterialParameterInfo ParameterInfo(VectorParameter->GetParameterName());
			if (!MaterialInstance->GetVectorParameterValue(ParameterInfo, Value))
			{
				// TODO: how to handle this?
			}
		}

		const uint32 MaskComponentCount = FGLTFMaterialUtilities::GetMaskComponentCount(*MaterialInput);

		if (MaskComponentCount > 0)
		{
			const FLinearColor Mask = FGLTFMaterialUtilities::GetMask(*MaterialInput);

			Value *= Mask;

			if (MaskComponentCount == 1)
			{
				const float ComponentValue = Value.R + Value.G + Value.B + Value.A;
				Value = { ComponentValue, ComponentValue, ComponentValue, ComponentValue };
			}
		}

		OutValue = Value;
		return true;
	}

	if (const UMaterialExpressionScalarParameter* ScalarParameter = ExactCast<UMaterialExpressionScalarParameter>(Expression))
	{
		float Value = ScalarParameter->DefaultValue;

		const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		if (MaterialInstance != nullptr)
		{
			const FHashedMaterialParameterInfo ParameterInfo(ScalarParameter->GetParameterName());
			if (!MaterialInstance->GetScalarParameterValue(ParameterInfo, Value))
			{
				// TODO: how to handle this?
			}
		}

		OutValue = { Value, Value, Value, Value };
		return true;
	}

	if (const UMaterialExpressionConstant4Vector* Constant4Vector = ExactCast<UMaterialExpressionConstant4Vector>(Expression))
	{
		OutValue = Constant4Vector->Constant;
		return true;
	}

	if (const UMaterialExpressionConstant3Vector* Constant3Vector = ExactCast<UMaterialExpressionConstant3Vector>(Expression))
	{
		OutValue = Constant3Vector->Constant;
		return true;
	}

	if (const UMaterialExpressionConstant2Vector* Constant2Vector = ExactCast<UMaterialExpressionConstant2Vector>(Expression))
	{
		OutValue = { Constant2Vector->R, Constant2Vector->G, 0, 0 };
		return true;
	}

	if (const UMaterialExpressionConstant* Constant = ExactCast<UMaterialExpressionConstant>(Expression))
	{
		OutValue = { Constant->R, Constant->R, Constant->R, Constant->R };
		return true;
	}

	return false;
}

bool FGLTFDelayedMaterialTask::TryGetConstantScalar(float& OutValue, const FMaterialPropertyEx& Property) const
{
	const bool bUseMaterialAttributes = Material->GetMaterial()->bUseMaterialAttributes;
	if (bUseMaterialAttributes)
	{
		// TODO: check if attribute property connected, i.e. Material->GetMaterial()->MaterialAttributes.IsConnected(Property)
		return false;
	}

	const FMaterialInput<float>* MaterialInput = FGLTFMaterialUtilities::GetInputForProperty<float>(Material, Property);
	if (MaterialInput == nullptr)
	{
		// TODO: report error
		return false;
	}

	if (MaterialInput->UseConstant)
	{
		OutValue = MaterialInput->Constant;
		return true;
	}

	const UMaterialExpression* Expression = MaterialInput->Expression;
	if (Expression == nullptr)
	{
		OutValue = FGLTFMaterialUtilities::GetPropertyDefaultValue(Property).X;
		return true;
	}

	if (const UMaterialExpressionVectorParameter* VectorParameter = ExactCast<UMaterialExpressionVectorParameter>(Expression))
	{
		FLinearColor Value = VectorParameter->DefaultValue;

		const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		if (MaterialInstance != nullptr)
		{
			const FHashedMaterialParameterInfo ParameterInfo(VectorParameter->GetParameterName());
			if (!MaterialInstance->GetVectorParameterValue(ParameterInfo, Value))
			{
				// TODO: how to handle this?
			}
		}

		const uint32 MaskComponentCount = FGLTFMaterialUtilities::GetMaskComponentCount(*MaterialInput);

		if (MaskComponentCount > 0)
		{
			const FLinearColor Mask = FGLTFMaterialUtilities::GetMask(*MaterialInput);
			Value *= Mask;
		}

		// TODO: is this a correct assumption, that the max component should be used as value?
		OutValue = Value.GetMax();
		return true;
	}

	if (const UMaterialExpressionScalarParameter* ScalarParameter = ExactCast<UMaterialExpressionScalarParameter>(Expression))
	{
		float Value = ScalarParameter->DefaultValue;

		const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		if (MaterialInstance != nullptr)
		{
			const FHashedMaterialParameterInfo ParameterInfo(ScalarParameter->GetParameterName());
			if (!MaterialInstance->GetScalarParameterValue(ParameterInfo, Value))
			{
				// TODO: how to handle this?
			}
		}

		OutValue = Value;
		return true;
	}

	if (const UMaterialExpressionConstant4Vector* Constant4Vector = ExactCast<UMaterialExpressionConstant4Vector>(Expression))
	{
		OutValue = Constant4Vector->Constant.R;
		return true;
	}

	if (const UMaterialExpressionConstant3Vector* Constant3Vector = ExactCast<UMaterialExpressionConstant3Vector>(Expression))
	{
		OutValue = Constant3Vector->Constant.R;
		return true;
	}

	if (const UMaterialExpressionConstant2Vector* Constant2Vector = ExactCast<UMaterialExpressionConstant2Vector>(Expression))
	{
		OutValue = Constant2Vector->R;
		return true;
	}

	if (const UMaterialExpressionConstant* Constant = ExactCast<UMaterialExpressionConstant>(Expression))
	{
		OutValue = Constant->R;
		return true;
	}

	return false;
}

bool FGLTFDelayedMaterialTask::TryGetSourceTexture(FGLTFJsonTextureInfo& OutTexInfo, const FMaterialPropertyEx& Property, const TArray<FLinearColor>& AllowedMasks) const
{
	const UTexture2D* Texture;
	int32 TexCoord;
	FGLTFJsonTextureTransform Transform;

	if (TryGetSourceTexture(Texture, TexCoord, Transform, Property, AllowedMasks))
	{
		OutTexInfo.Index = Builder.AddUniqueTexture(Texture, FGLTFMaterialUtilities::IsSRGB(Property));
		OutTexInfo.TexCoord = TexCoord;
		OutTexInfo.Transform = Transform;
		return true;
	}

	return false;
}

bool FGLTFDelayedMaterialTask::TryGetSourceTexture(const UTexture2D*& OutTexture, int32& OutTexCoord, FGLTFJsonTextureTransform& OutTransform, const FMaterialPropertyEx& Property, const TArray<FLinearColor>& AllowedMasks) const
{
	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		return false;
	}

	const FExpressionInput* MaterialInput = FGLTFMaterialUtilities::GetInputForProperty(Material, Property);
	if (MaterialInput == nullptr)
	{
		// TODO: report error
		return false;
	}

	const UMaterialExpression* Expression = MaterialInput->Expression;
	if (Expression == nullptr)
	{
		return false;
	}

	const FLinearColor InputMask = FGLTFMaterialUtilities::GetMask(*MaterialInput);
	if (AllowedMasks.Num() > 0 && !AllowedMasks.Contains(InputMask))
	{
		return false;
	}

	// TODO: add support or warning for texture sampler settings that override texture asset addressing (i.e. wrap, clamp etc)?

	if (const UMaterialExpressionTextureSampleParameter2D* TextureParameter = ExactCast<UMaterialExpressionTextureSampleParameter2D>(Expression))
	{
		// TODO: handle non-default TextureParameter->SamplerType and TextureParameter->SamplerSource

		UTexture* ParameterValue = TextureParameter->Texture;

		const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		if (MaterialInstance != nullptr)
		{
			const FHashedMaterialParameterInfo ParameterInfo(TextureParameter->GetParameterName());
			if (!MaterialInstance->GetTextureParameterValue(ParameterInfo, ParameterValue))
			{
				// TODO: how to handle this?
			}
		}

		// TODO: add support for UTextureRenderTarget2D?
		OutTexture = Cast<UTexture2D>(ParameterValue);

		if (OutTexture == nullptr)
		{
			if (ParameterValue == nullptr)
			{
				// TODO: report error (no texture parameter assigned)
			}
			else
			{
				// TODO: report error (incorrect texture type)
			}
			return false;
		}

		if (!FGLTFMaterialUtilities::TryGetTextureCoordinateIndex(TextureParameter, OutTexCoord, OutTransform))
		{
			// TODO: report error (failed to identify texture coordinate index)
			return false;
		}

		if (!Builder.ExportOptions->bExportTextureTransforms && !OutTransform.IsExactlyDefault())
		{
			Builder.LogWarning(FString::Printf(
				TEXT("Texture coordinates [%d] in %s for material %s are transformed, but texture transform is disabled by export options"),
				OutTexCoord,
				*Property.ToString(),
				*Material->GetName()));
			OutTransform = {};
		}

		return true;
	}

	if (const UMaterialExpressionTextureSample* TextureSampler = ExactCast<UMaterialExpressionTextureSample>(Expression))
	{
		// TODO: handle non-default TextureSampler->SamplerType and TextureSampler->SamplerSource

		// TODO: add support for texture object input expression

		// TODO: add support for UTextureRenderTarget2D?
		OutTexture = Cast<UTexture2D>(TextureSampler->Texture);

		if (OutTexture == nullptr)
		{
			if (TextureSampler->Texture == nullptr)
			{
				// TODO: report error (no texture sample assigned)
			}
			else
			{
				// TODO: report error (incorrect texture type)
			}
			return false;
		}

		if (!FGLTFMaterialUtilities::TryGetTextureCoordinateIndex(TextureSampler, OutTexCoord, OutTransform))
		{
			// TODO: report error (failed to identify texture coordinate index)
			return false;
		}

		if (!Builder.ExportOptions->bExportTextureTransforms && !OutTransform.IsExactlyDefault())
		{
			Builder.LogWarning(FString::Printf(
				TEXT("Texture coordinates [%d] in %s for material %s are transformed, but texture transform is disabled by export options"),
				OutTexCoord,
				*Property.ToString(),
				*Material->GetName()));
			OutTransform = {};
		}

		return true;
	}

	return false;
}

bool FGLTFDelayedMaterialTask::TryGetBakedMaterialProperty(FGLTFJsonTextureInfo& OutTexInfo, FGLTFJsonColor3& OutConstant, const FMaterialPropertyEx& Property, const FString& PropertyName)
{
	if (Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::Disabled)
	{
		Builder.LogWarning(FString::Printf(
			TEXT("%s for material %s needs to bake, but material baking is disabled by export options"),
			*Property.ToString(),
			*Material->GetName()));
		return false;
	}

	FGLTFPropertyBakeOutput PropertyBakeOutput = BakeMaterialProperty(Property, OutTexInfo.TexCoord);

	if (PropertyBakeOutput.bIsConstant)
	{
		OutConstant = FGLTFCoreUtilities::ConvertColor3(PropertyBakeOutput.ConstantValue);
		return true;
	}

	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		OutTexInfo.Index = nullptr;
		return true;
	}

	if (StoreBakedPropertyTexture(OutTexInfo, PropertyBakeOutput, PropertyName))
	{
		OutConstant = FGLTFJsonColor3::White; // make sure property is not zero
		return true;
	}

	return false;
}

bool FGLTFDelayedMaterialTask::TryGetBakedMaterialProperty(FGLTFJsonTextureInfo& OutTexInfo, FGLTFJsonColor4& OutConstant, const FMaterialPropertyEx& Property, const FString& PropertyName)
{
	if (Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::Disabled)
	{
		Builder.LogWarning(FString::Printf(
			TEXT("%s for material %s needs to bake, but material baking is disabled by export options"),
			*Property.ToString(),
			*Material->GetName()));
		return false;
	}

	FGLTFPropertyBakeOutput PropertyBakeOutput = BakeMaterialProperty(Property, OutTexInfo.TexCoord);

	if (PropertyBakeOutput.bIsConstant)
	{
		OutConstant = FGLTFCoreUtilities::ConvertColor(PropertyBakeOutput.ConstantValue);
		return true;
	}

	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		OutTexInfo.Index = nullptr;
		return true;
	}

	if (StoreBakedPropertyTexture(OutTexInfo, PropertyBakeOutput, PropertyName))
	{
		OutConstant = FGLTFJsonColor4::White; // make sure property is not zero
		return true;
	}

	return false;
}

inline bool FGLTFDelayedMaterialTask::TryGetBakedMaterialProperty(FGLTFJsonTextureInfo& OutTexInfo, float& OutConstant, const FMaterialPropertyEx& Property, const FString& PropertyName)
{
	if (Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::Disabled)
	{
		Builder.LogWarning(FString::Printf(
			TEXT("%s for material %s needs to bake, but material baking is disabled by export options"),
			*Property.ToString(),
			*Material->GetName()));
		return false;
	}

	FGLTFPropertyBakeOutput PropertyBakeOutput = BakeMaterialProperty(Property, OutTexInfo.TexCoord);

	if (PropertyBakeOutput.bIsConstant)
	{
		OutConstant = PropertyBakeOutput.ConstantValue.R;
		return true;
	}

	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		OutTexInfo.Index = nullptr;
		return true;
	}

	if (StoreBakedPropertyTexture(OutTexInfo, PropertyBakeOutput, PropertyName))
	{
		OutConstant = 1; // make sure property is not zero
		return true;
	}

	return false;
}

bool FGLTFDelayedMaterialTask::TryGetBakedMaterialProperty(FGLTFJsonTextureInfo& OutTexInfo, const FMaterialPropertyEx& Property, const FString& PropertyName)
{
	if (Builder.ExportOptions->BakeMaterialInputs == EGLTFMaterialBakeMode::Disabled)
	{
		Builder.LogWarning(FString::Printf(
			TEXT("%s for material %s needs to bake, but material baking is disabled by export options"),
			*Property.ToString(),
			*Material->GetName()));
		return false;
	}

	FGLTFPropertyBakeOutput PropertyBakeOutput = BakeMaterialProperty(Property, OutTexInfo.TexCoord);

	if (!PropertyBakeOutput.bIsConstant)
	{
		if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
		{
			OutTexInfo.Index = nullptr;
			return true;
		}

		return StoreBakedPropertyTexture(OutTexInfo, PropertyBakeOutput, PropertyName);
	}

	const FVector4f MaskedConstant = FVector4f(PropertyBakeOutput.ConstantValue) * FGLTFMaterialUtilities::GetPropertyMask(Property);
	if (MaskedConstant == FGLTFMaterialUtilities::GetPropertyDefaultValue(Property))
	{
		// Constant value is the same as the property's default so we can set gltf to default.
		OutTexInfo.Index = nullptr;
		return true;
	}

	if (FGLTFMaterialUtilities::IsNormalMap(Property))
	{
		// TODO: In some cases baking normal can result in constant vector that differs slight from default (i.e 0,0,1).
		// Yet often, when looking at such a material, it should be exactly default. Needs further investigation.
		// Maybe because of incorrect sRGB conversion? For now, assume a constant normal is always default.
		OutTexInfo.Index = nullptr;
		return true;
	}

	// TODO: let function fail and investigate why in some cases a constant baking result is returned for a property
	// that is non-constant. This happens (for example) when baking AmbientOcclusion for a translucent material,
	// even though the same material when set to opaque will properly bake AmbientOcclusion to a texture.
	// For now, create a 1x1 texture with the constant value.

	FGLTFJsonTexture* Texture = FGLTFMaterialUtilities::AddTexture(
		Builder,
		PropertyBakeOutput.Pixels,
		PropertyBakeOutput.Size,
		true, // NOTE: we can ignore alpha in everything but TryGetBaseColorAndOpacity
		false, // Normal and ClearCoatBottomNormal are handled above
		GetBakedTextureName(PropertyName),
		TA_Clamp,
		TF_Nearest);

	OutTexInfo.Index = Texture;
	return true;
}

FGLTFPropertyBakeOutput FGLTFDelayedMaterialTask::BakeMaterialProperty(const FMaterialPropertyEx& Property, int32& OutTexCoord)
{
	const FIntPoint TextureSize = Builder.GetBakeSizeForMaterialProperty(Material, GetPropertyGroup(Property));
	return BakeMaterialProperty(Property, OutTexCoord, TextureSize, true);
}

FGLTFPropertyBakeOutput FGLTFDelayedMaterialTask::BakeMaterialProperty(const FMaterialPropertyEx& Property, int32& OutTexCoord, const FIntPoint& TextureSize, bool bFillAlpha)
{
	if (MeshData == nullptr)
	{
		FGLTFIndexArray TexCoords;
		FGLTFMaterialUtilities::GetAllTextureCoordinateIndices(Material, Property, TexCoords);

		if (TexCoords.Num() > 0)
		{
			OutTexCoord = TexCoords[0];

			if (TexCoords.Num() > 1)
			{
				Builder.LogWarning(FString::Printf(
					TEXT("%s for material %s uses multiple texture coordinates (%s), baked texture will be sampled using only the first (%d)"),
					*Property.ToString(),
					*Material->GetName(),
					*FString::JoinBy(TexCoords, TEXT(", "), FString::FromInt),
					OutTexCoord));
			}
		}
		else
		{
			OutTexCoord = 0;
		}
	}
	else
	{
		OutTexCoord = MeshData->BakeUsingTexCoord;
		MeshDataBakedProperties.Add(Property);
	}

	// TODO: add support for calculating the ideal resolution to use for baking based on connected (texture) nodes

	return FGLTFMaterialUtilities::BakeMaterialProperty(
		TextureSize,
		Property,
		Material,
		OutTexCoord,
		MeshData,
		SectionIndices,
		bFillAlpha,
		Builder.ExportOptions->bAdjustNormalmaps);
}

bool FGLTFDelayedMaterialTask::StoreBakedPropertyTexture(FGLTFJsonTextureInfo& OutTexInfo, FGLTFPropertyBakeOutput& PropertyBakeOutput, const FString& PropertyName) const
{
	const TextureAddress TextureAddress = Builder.GetBakeTilingForMaterialProperty(Material, GetPropertyGroup(PropertyBakeOutput.Property));
	const TextureFilter TextureFilter = Builder.GetBakeFilterForMaterialProperty(Material, GetPropertyGroup(PropertyBakeOutput.Property));

	FGLTFJsonTexture* Texture = FGLTFMaterialUtilities::AddTexture(
		Builder,
		PropertyBakeOutput.Pixels,
		PropertyBakeOutput.Size,
		true, // NOTE: we can ignore alpha in everything but TryGetBaseColorAndOpacity
		FGLTFMaterialUtilities::IsNormalMap(PropertyBakeOutput.Property),
		GetBakedTextureName(PropertyName),
		TextureAddress,
		TextureFilter);

	OutTexInfo.Index = Texture;
	return true;
}

EGLTFMaterialPropertyGroup FGLTFDelayedMaterialTask::GetPropertyGroup(const FMaterialPropertyEx& Property)
{
	switch (Property.Type)
	{
		case MP_BaseColor:
		case MP_Opacity:
		case MP_OpacityMask:
			return EGLTFMaterialPropertyGroup::BaseColorOpacity;
		case MP_Metallic:
		case MP_Roughness:
			return EGLTFMaterialPropertyGroup::MetallicRoughness;
		case MP_EmissiveColor:
			return EGLTFMaterialPropertyGroup::EmissiveColor;
		case MP_Normal:
			return EGLTFMaterialPropertyGroup::Normal;
		case MP_AmbientOcclusion:
			return EGLTFMaterialPropertyGroup::AmbientOcclusion;
		case MP_CustomData0:
		case MP_CustomData1:
			return EGLTFMaterialPropertyGroup::ClearCoatRoughness;
		case MP_CustomOutput:
			if (Property == FMaterialPropertyEx::ClearCoatBottomNormal)
			{
				return EGLTFMaterialPropertyGroup::ClearCoatBottomNormal;
			}
		default:
			return EGLTFMaterialPropertyGroup::None;
	}
}

template <typename CallbackType>
void FGLTFDelayedMaterialTask::CombinePixels(const TArray<FColor>& FirstPixels, const TArray<FColor>& SecondPixels, TArray<FColor>& OutPixels, CallbackType Callback)
{
	const int32 Count = FMath::Max(FirstPixels.Num(), SecondPixels.Num());
	OutPixels.AddUninitialized(Count);

	if (FirstPixels.Num() == 1)
	{
		const FColor& FirstPixel = FirstPixels[0];
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FColor& SecondPixel = SecondPixels[Index];
			OutPixels[Index] = Callback(FirstPixel, SecondPixel);
		}
	}
	else if (SecondPixels.Num() == 1)
	{
		const FColor& SecondPixel = SecondPixels[0];
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FColor& FirstPixel = FirstPixels[Index];
			OutPixels[Index] = Callback(FirstPixel, SecondPixel);
		}
	}
	else
	{
		check(FirstPixels.Num() == SecondPixels.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FColor& FirstPixel = FirstPixels[Index];
			const FColor& SecondPixel = SecondPixels[Index];
			OutPixels[Index] = Callback(FirstPixel, SecondPixel);
		}
	}
}

#endif
