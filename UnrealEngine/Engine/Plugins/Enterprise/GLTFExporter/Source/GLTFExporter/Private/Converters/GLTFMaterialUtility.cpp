// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFMaterialUtility.h"
#include "Converters/GLTFTextureUtility.h"
#include "Converters/GLTFNameUtility.h"
#include "Utilities/GLTFProxyMaterialUtilities.h"
#include "Misc/DefaultValueHelper.h"
#if WITH_EDITOR
#include "GLTFMaterialAnalyzer.h"
#include "IGLTFMaterialBakingModule.h"
#include "GLTFMaterialBakingStructures.h"
#endif
#include "Modules/ModuleManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionClearCoatNormalCustomOutput.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"

UMaterialInterface* FGLTFMaterialUtility::GetDefaultMaterial()
{
	static UMaterial* DefaultMaterial = FGLTFProxyMaterialUtilities::GetBaseMaterial(EGLTFJsonShadingModel::Default);
	return DefaultMaterial;
}

#if WITH_EDITOR

bool FGLTFMaterialUtility::IsNormalMap(const FGLTFMaterialPropertyEx& Property)
{
	return Property == MP_Normal || Property == TEXT("ClearCoatBottomNormal");
}

bool FGLTFMaterialUtility::IsSRGB(const FGLTFMaterialPropertyEx& Property)
{
	return Property == MP_BaseColor || Property == MP_EmissiveColor || Property == MP_SubsurfaceColor || Property == TEXT("TransmittanceColor");
}

FGuid FGLTFMaterialUtility::GetAttributeID(const FGLTFMaterialPropertyEx& Property)
{
	return Property.IsCustomOutput()
		? FMaterialAttributeDefinitionMap::GetCustomAttributeID(Property.CustomOutput.ToString())
		: FMaterialAttributeDefinitionMap::GetID(Property.Type);
}

FGuid FGLTFMaterialUtility::GetAttributeIDChecked(const FGLTFMaterialPropertyEx& Property)
{
	const FGuid AttributeID = GetAttributeID(Property);
	check(AttributeID != FMaterialAttributeDefinitionMap::GetDefaultID());
	return AttributeID;
}

FVector4f FGLTFMaterialUtility::GetPropertyDefaultValue(const FGLTFMaterialPropertyEx& Property)
{
	return FMaterialAttributeDefinitionMap::GetDefaultValue(GetAttributeIDChecked(Property));
}

FVector4f FGLTFMaterialUtility::GetPropertyMask(const FGLTFMaterialPropertyEx& Property)
{
	switch (FMaterialAttributeDefinitionMap::GetValueType(GetAttributeIDChecked(Property)))
	{
		case MCT_Float:
		case MCT_Float1: return FVector4f(1, 0, 0, 0);
		case MCT_Float2: return FVector4f(1, 1, 0, 0);
		case MCT_Float3: return FVector4f(1, 1, 1, 0);
		case MCT_Float4: return FVector4f(1, 1, 1, 1);
		default:
			checkNoEntry();
			return FVector4f();
	}
}

const FExpressionInput* FGLTFMaterialUtility::GetInputForProperty(const UMaterialInterface* Material, const FGLTFMaterialPropertyEx& Property)
{
	if (Property.IsCustomOutput())
	{
		const UMaterialExpressionCustomOutput* CustomOutput = GetCustomOutputByName(Material, Property.CustomOutput.ToString());
		return CustomOutput != nullptr ? &CastChecked<UMaterialExpressionClearCoatNormalCustomOutput>(CustomOutput)->Input : nullptr;
	}

	UMaterial* UnderlyingMaterial = const_cast<UMaterial*>(Material->GetMaterial());
	return UnderlyingMaterial->GetExpressionInputForProperty(Property.Type);
}

const UMaterialExpressionCustomOutput* FGLTFMaterialUtility::GetCustomOutputByName(const UMaterialInterface* Material, const FString& Name)
{
	// TODO: should we also search inside material functions and attribute layers?

	for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetMaterial()->GetExpressions())
	{
		const UMaterialExpressionCustomOutput* CustomOutput = Cast<UMaterialExpressionCustomOutput>(Expression);
		if (CustomOutput != nullptr && CustomOutput->GetDisplayName() == Name)
		{
			return CustomOutput;
		}
	}

	return nullptr;
}

FGLTFPropertyBakeOutput FGLTFMaterialUtility::BakeMaterialProperty(const FIntPoint& OutputSize, const FGLTFMaterialPropertyEx& Property, const UMaterialInterface* Material, int32 TexCoord, const FGLTFMeshData* MeshData, const FGLTFIndexArray& MeshSectionIndices, bool bFillAlpha, bool bAdjustNormalmaps)
{
	FGLTFMeshRenderData MeshSet;
	MeshSet.TextureCoordinateBox = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
	MeshSet.TextureCoordinateIndex = TexCoord;
	MeshSet.MaterialIndices = MeshSectionIndices; // NOTE: MaterialIndices is actually section indices
	if (MeshData != nullptr)
	{
		MeshSet.MeshDescription = const_cast<FMeshDescription*>(&MeshData->Description);
		MeshSet.LightMap = MeshData->LightMap;
		MeshSet.LightMapIndex = MeshData->LightMapTexCoord;
		MeshSet.LightmapResourceCluster = MeshData->LightMapResourceCluster;
		MeshSet.PrimitiveData = &MeshData->PrimitiveData;
	}

	FGLTFMaterialDataEx MatSet;
	MatSet.Material = const_cast<UMaterialInterface*>(Material);
	MatSet.PropertySizes.Add(Property, OutputSize);
	MatSet.bTangentSpaceNormal = true;

	TArray<FGLTFMeshRenderData*> MeshSettings;
	TArray<FGLTFMaterialDataEx*> MatSettings;
	MeshSettings.Add(&MeshSet);
	MatSettings.Add(&MatSet);

	TArray<FGLTFBakeOutputEx> BakeOutputs;
	IGLTFMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IGLTFMaterialBakingModule>("GLTFMaterialBaking");

	Module.SetLinearBake(true);
	Module.BakeMaterials(MatSettings, MeshSettings, BakeOutputs);
	const bool bIsLinearBake = Module.IsLinearBake(Property);
	Module.SetLinearBake(false);

	FGLTFBakeOutputEx& BakeOutput = BakeOutputs[0];

	TGLTFSharedArray<FColor> BakedPixels = MakeShared<TArray<FColor>>(MoveTemp(BakeOutput.PropertyData.FindChecked(Property)));
	const FIntPoint BakedSize = BakeOutput.PropertySizes.FindChecked(Property);
	const float EmissiveScale = BakeOutput.EmissiveScale;

	if (bFillAlpha)
	{
		// NOTE: alpha is 0 by default after baking a property, but we prefer 255 (1.0).
		// It makes it easier to view the exported textures.
		for (FColor& Pixel: *BakedPixels)
		{
			Pixel.A = 255;
		}
	}

	if (bAdjustNormalmaps && IsNormalMap(Property))
	{
		// TODO: add support for adjusting normals in baking module instead
		FGLTFTextureUtility::FlipGreenChannel(*BakedPixels);
	}

	bool bFromSRGB = !bIsLinearBake;
	bool bToSRGB = IsSRGB(Property);
	FGLTFTextureUtility::TransformColorSpace(*BakedPixels, bFromSRGB, bToSRGB);

	FGLTFPropertyBakeOutput PropertyBakeOutput(Property, PF_B8G8R8A8, BakedPixels, BakedSize, EmissiveScale, !bIsLinearBake);

	if (BakedPixels->Num() == 1)
	{
		const FColor& Pixel = (*BakedPixels)[0];

		PropertyBakeOutput.bIsConstant = true;
		PropertyBakeOutput.ConstantValue = bToSRGB ? FLinearColor(Pixel) : Pixel.ReinterpretAsLinear();
	}

	return PropertyBakeOutput;
}

FGLTFJsonTexture* FGLTFMaterialUtility::AddTexture(FGLTFConvertBuilder& Builder, TGLTFSharedArray<FColor>& Pixels, const FIntPoint& TextureSize, bool bIgnoreAlpha, bool bIsNormalMap, const FString& TextureName, EGLTFJsonTextureFilter MinFilter, EGLTFJsonTextureFilter MagFilter, EGLTFJsonTextureWrap WrapS, EGLTFJsonTextureWrap WrapT)
{
	// TODO: maybe we should reuse existing samplers?
	FGLTFJsonSampler* JsonSampler = Builder.AddSampler();
	JsonSampler->Name = TextureName;
	JsonSampler->MinFilter = MinFilter;
	JsonSampler->MagFilter = MagFilter;
	JsonSampler->WrapS = WrapS;
	JsonSampler->WrapT = WrapT;

	// TODO: reuse same texture index when image is the same
	FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
	JsonTexture->Name = TextureName;
	JsonTexture->Sampler = JsonSampler;
	JsonTexture->Source = Builder.AddUniqueImage(Pixels, TextureSize, bIgnoreAlpha, bIsNormalMap ? EGLTFTextureType::Normalmaps : EGLTFTextureType::None, TextureName);

	return JsonTexture;
}

FLinearColor FGLTFMaterialUtility::GetMask(const FExpressionInput& ExpressionInput)
{
	return FLinearColor(ExpressionInput.MaskR, ExpressionInput.MaskG, ExpressionInput.MaskB, ExpressionInput.MaskA);
}

uint32 FGLTFMaterialUtility::GetMaskComponentCount(const FExpressionInput& ExpressionInput)
{
	return ExpressionInput.MaskR + ExpressionInput.MaskG + ExpressionInput.MaskB + ExpressionInput.MaskA;
}

bool FGLTFMaterialUtility::TryGetTextureCoordinateIndex(const UMaterialExpressionTextureSample* TextureSampler, int32& TexCoord, FGLTFJsonTextureTransform& Transform)
{
	const UMaterialExpression* Expression = TextureSampler->Coordinates.Expression;
	if (Expression == nullptr)
	{
		TexCoord = TextureSampler->ConstCoordinate;
		Transform = {};
		return true;
	}

	if (const UMaterialExpressionTextureCoordinate* TextureCoordinate = Cast<UMaterialExpressionTextureCoordinate>(Expression))
	{
		TexCoord = TextureCoordinate->CoordinateIndex;
		Transform.Offset.X = TextureCoordinate->UnMirrorU ? TextureCoordinate->UTiling * 0.5 : 0.0;
		Transform.Offset.Y = TextureCoordinate->UnMirrorV ? TextureCoordinate->VTiling * 0.5 : 0.0;
		Transform.Scale.X = TextureCoordinate->UTiling * (TextureCoordinate->UnMirrorU ? 0.5 : 1.0);
		Transform.Scale.Y = TextureCoordinate->VTiling * (TextureCoordinate->UnMirrorV ? 0.5 : 1.0);
		Transform.Rotation = 0;
		return true;
	}

	// TODO: add support for advanced expression tree (ex UMaterialExpressionTextureCoordinate -> UMaterialExpressionMultiply -> UMaterialExpressionAdd)

	return false;
}

void FGLTFMaterialUtility::GetAllTextureCoordinateIndices(const UMaterialInterface* InMaterial, const FGLTFMaterialPropertyEx& InProperty, FGLTFIndexArray& OutTexCoords)
{
	FGLTFMaterialAnalysis Analysis;
	AnalyzeMaterialProperty(InMaterial, InProperty, Analysis);

	const TBitArray<>& TexCoords = Analysis.TextureCoordinates;
	for (int32 Index = 0; Index < TexCoords.Num(); Index++)
	{
		if (TexCoords[Index])
		{
			OutTexCoords.Add(Index);
		}
	}
}

void FGLTFMaterialUtility::AnalyzeMaterialProperty(const UMaterialInterface* InMaterial, const FGLTFMaterialPropertyEx& InProperty, FGLTFMaterialAnalysis& OutAnalysis)
{
	if (GetInputForProperty(InMaterial, InProperty) == nullptr)
	{
		OutAnalysis = FGLTFMaterialAnalysis();
		return;
	}

	UGLTFMaterialAnalyzer::AnalyzeMaterialPropertyEx(InMaterial, InProperty.Type, InProperty.CustomOutput.ToString(), OutAnalysis);
}

FMaterialShadingModelField FGLTFMaterialUtility::EvaluateShadingModelExpression(const UMaterialInterface* Material)
{
	FGLTFMaterialAnalysis Analysis;
	AnalyzeMaterialProperty(Material, MP_ShadingModel, Analysis);

	int32 Value;
	if (FDefaultValueHelper::ParseInt(Analysis.ParameterCode, Value))
	{
		return static_cast<EMaterialShadingModel>(Value);
	}

	return Analysis.ShadingModels;
}

#endif

EMaterialShadingModel FGLTFMaterialUtility::GetRichestShadingModel(const FMaterialShadingModelField& ShadingModels)
{
	if (ShadingModels.HasShadingModel(MSM_ClearCoat))
	{
		return MSM_ClearCoat;
	}

	if (ShadingModels.HasShadingModel(MSM_DefaultLit))
	{
		return MSM_DefaultLit;
	}

	if (ShadingModels.HasShadingModel(MSM_Unlit))
	{
		return MSM_Unlit;
	}

	// TODO: add more shading models when conversion supported

	return ShadingModels.GetFirstShadingModel();
}

FString FGLTFMaterialUtility::ShadingModelsToString(const FMaterialShadingModelField& ShadingModels)
{
	FString Result;

	for (uint32 Index = 0; Index < MSM_NUM; Index++)
	{
		const EMaterialShadingModel ShadingModel = static_cast<EMaterialShadingModel>(Index);
		if (ShadingModels.HasShadingModel(ShadingModel))
		{
			FString Name = FGLTFNameUtility::GetName(ShadingModel);
			Result += Result.IsEmpty() ? Name : TEXT(", ") + Name;
		}
	}

	return Result;
}

bool FGLTFMaterialUtility::NeedsMeshData(const UMaterialInterface* Material)
{
#if WITH_EDITOR
	if (Material != nullptr && !FGLTFProxyMaterialUtilities::IsProxyMaterial(Material))
	{
		// TODO: only analyze properties that will be needed for this specific material
		const TArray<FGLTFMaterialPropertyEx> Properties =
		{
			MP_BaseColor,
			MP_EmissiveColor,
			MP_Opacity,
			MP_OpacityMask,
			MP_Metallic,
			MP_Roughness,
			MP_Normal,
			MP_AmbientOcclusion,
			MP_CustomData0,
			MP_CustomData1,
			TEXT("ClearCoatBottomNormal"),
		};

		bool bNeedsMeshData = false;
		FGLTFMaterialAnalysis Analysis;

		// TODO: optimize baking by separating need for vertex data and primitive data

		for (const FGLTFMaterialPropertyEx& Property: Properties)
		{
			AnalyzeMaterialProperty(Material, Property, Analysis);
			bNeedsMeshData |= Analysis.bRequiresVertexData;
			bNeedsMeshData |= Analysis.bRequiresPrimitiveData;
		}

		return bNeedsMeshData;
	}
#endif

	return false;
}

bool FGLTFMaterialUtility::NeedsMeshData(const TArray<const UMaterialInterface*>& Materials)
{
#if WITH_EDITOR
	for (const UMaterialInterface* Material: Materials)
	{
		if (NeedsMeshData(Material))
		{
			return true;
		}
	}
#endif

	return false;
}
