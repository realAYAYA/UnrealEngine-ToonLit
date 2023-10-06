// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFMaterialUtilities.h"
#include "Converters/GLTFTextureUtilities.h"
#include "Converters/GLTFNameUtilities.h"
#include "Converters/GLTFProxyMaterialCompiler.h"
#include "Utilities/GLTFProxyMaterialUtilities.h"
#include "Materials/Material.h"
#include "Misc/DefaultValueHelper.h"
#if WITH_EDITOR
#include "IMaterialBakingModule.h"
#include "MaterialBakingStructures.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#endif
#include "Engine/RendererSettings.h"
#include "Modules/ModuleManager.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"

UMaterialInterface* FGLTFMaterialUtilities::GetDefaultMaterial()
{
	static UMaterial* DefaultMaterial = FGLTFProxyMaterialUtilities::GetBaseMaterial(EGLTFJsonShadingModel::Default);
	return DefaultMaterial;
}

bool FGLTFMaterialUtilities::IsClearCoatBottomNormalEnabled()
{
	return GetDefault<URendererSettings>()->bClearCoatEnableSecondNormal != 0;
}

#if WITH_EDITOR

bool FGLTFMaterialUtilities::IsNormalMap(const FMaterialPropertyEx& Property)
{
	return Property == MP_Normal || Property == FMaterialPropertyEx::ClearCoatBottomNormal;
}

bool FGLTFMaterialUtilities::IsSRGB(const FMaterialPropertyEx& Property)
{
	return Property == MP_BaseColor || Property == MP_EmissiveColor || Property == MP_SubsurfaceColor || Property == FMaterialPropertyEx::TransmittanceColor;
}

FGuid FGLTFMaterialUtilities::GetAttributeID(const FMaterialPropertyEx& Property)
{
	return Property.IsCustomOutput()
		? FMaterialAttributeDefinitionMap::GetCustomAttributeID(Property.CustomOutput.ToString())
		: FMaterialAttributeDefinitionMap::GetID(Property.Type);
}

FGuid FGLTFMaterialUtilities::GetAttributeIDChecked(const FMaterialPropertyEx& Property)
{
	const FGuid AttributeID = GetAttributeID(Property);
	check(AttributeID != FMaterialAttributeDefinitionMap::GetDefaultID());
	return AttributeID;
}

FVector4f FGLTFMaterialUtilities::GetPropertyDefaultValue(const FMaterialPropertyEx& Property)
{
	return FMaterialAttributeDefinitionMap::GetDefaultValue(GetAttributeIDChecked(Property));
}

FVector4f FGLTFMaterialUtilities::GetPropertyMask(const FMaterialPropertyEx& Property)
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

const FExpressionInput* FGLTFMaterialUtilities::GetInputForProperty(const UMaterialInterface* Material, const FMaterialPropertyEx& Property)
{
	if (Property.IsCustomOutput())
	{
		const FString FunctionName = Property.CustomOutput.ToString();
		const UMaterialExpressionCustomOutput* CustomOutput = GetCustomOutputByName(Material, FunctionName);
		if (CustomOutput == nullptr)
		{
			return nullptr;
		}

		// Assume custom outputs always have a single input (which is true for all supported custom outputs)
		return const_cast<UMaterialExpressionCustomOutput*>(CustomOutput)->GetInput(0);
	}

	UMaterial* UnderlyingMaterial = const_cast<UMaterial*>(Material->GetMaterial());
	return UnderlyingMaterial->GetExpressionInputForProperty(Property.Type);
}

UMaterialExpressionCustomOutput* FGLTFMaterialUtilities::GetCustomOutputByName(const UMaterialInterface* Material, const FString& FunctionName)
{
	for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetMaterial()->GetExpressions())
	{
		UMaterialExpressionCustomOutput* CustomOutput = Cast<UMaterialExpressionCustomOutput>(Expression);
		if (CustomOutput != nullptr && CustomOutput->GetFunctionName() == FunctionName)
		{
			return CustomOutput;
		}
	}

	return nullptr;
}

FGLTFPropertyBakeOutput FGLTFMaterialUtilities::BakeMaterialProperty(const FIntPoint& OutputSize, const FMaterialPropertyEx& Property, const UMaterialInterface* Material, const FBox2f& TexCoordBounds, int32 TexCoordIndex, const FGLTFMeshData* MeshData, const FGLTFIndexArray& MeshSectionIndices, bool bFillAlpha, bool bAdjustNormalmaps)
{
	FMeshData MeshSet;
	MeshSet.TextureCoordinateBox = FBox2D(TexCoordBounds);
	MeshSet.TextureCoordinateIndex = TexCoordIndex;
	MeshSet.MaterialIndices = MeshSectionIndices; // NOTE: MaterialIndices is actually section indices
	if (MeshData != nullptr)
	{
		MeshSet.MeshDescription = const_cast<FMeshDescription*>(&MeshData->Description);
		MeshSet.LightMap = MeshData->LightMap;
		MeshSet.LightMapIndex = MeshData->LightMapTexCoord;
		MeshSet.LightmapResourceCluster = MeshData->LightMapResourceCluster;
		MeshSet.PrimitiveData = &MeshData->PrimitiveData;
	}

	FMaterialDataEx MatSet;
	MatSet.Material = const_cast<UMaterialInterface*>(Material);
	MatSet.PropertySizes.Add(Property, OutputSize);
	MatSet.bTangentSpaceNormal = true;

	TArray<FMeshData*> MeshSettings;
	TArray<FMaterialDataEx*> MatSettings;
	MeshSettings.Add(&MeshSet);
	MatSettings.Add(&MatSet);

	TArray<FBakeOutputEx> BakeOutputs;
	IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>("MaterialBaking");

	Module.SetLinearBake(true);
	Module.BakeMaterials(MatSettings, MeshSettings, BakeOutputs);
	const bool bIsLinearBake = Module.IsLinearBake(Property);
	Module.SetLinearBake(false);

	FBakeOutputEx& BakeOutput = BakeOutputs[0];

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
		FGLTFTextureUtilities::FlipGreenChannel(*BakedPixels);
	}

	bool bFromSRGB = !bIsLinearBake;
	FGLTFPropertyBakeOutput PropertyBakeOutput(BakedPixels, BakedSize, EmissiveScale, bFromSRGB);

	bool bToSRGB = IsSRGB(Property);
	FGLTFTextureUtilities::TransformColorSpace(*BakedPixels, bFromSRGB, bToSRGB);

	return PropertyBakeOutput;
}

FGLTFJsonTexture* FGLTFMaterialUtilities::AddTexture(FGLTFConvertBuilder& Builder, TGLTFSharedArray<FColor>& Pixels, const FIntPoint& TextureSize, bool bIgnoreAlpha, bool bIsNormalMap, const FString& TextureName, TextureAddress TextureAddress, TextureFilter TextureFilter)
{
	// TODO: reuse same texture index when image is the same
	FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
	JsonTexture->Name = TextureName;
	JsonTexture->Sampler = Builder.AddUniqueSampler(TextureAddress, TextureFilter);
	JsonTexture->Source = Builder.AddUniqueImage(Pixels, TextureSize, bIgnoreAlpha, TextureName);

	return JsonTexture;
}

FLinearColor FGLTFMaterialUtilities::GetMask(const FExpressionInput& ExpressionInput)
{
	return FLinearColor(
		static_cast<float>(ExpressionInput.MaskR),
		static_cast<float>(ExpressionInput.MaskG),
		static_cast<float>(ExpressionInput.MaskB),
		static_cast<float>(ExpressionInput.MaskA)
	);
}

uint32 FGLTFMaterialUtilities::GetMaskComponentCount(const FExpressionInput& ExpressionInput)
{
	return ExpressionInput.MaskR + ExpressionInput.MaskG + ExpressionInput.MaskB + ExpressionInput.MaskA;
}

bool FGLTFMaterialUtilities::TryGetTextureCoordinateIndex(const UMaterialExpressionTextureSample* TextureSampler, int32& TexCoord, FGLTFJsonTextureTransform& Transform)
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
		Transform.Offset.X = TextureCoordinate->UnMirrorU ? TextureCoordinate->UTiling * 0.5f : 0.0f;
		Transform.Offset.Y = TextureCoordinate->UnMirrorV ? TextureCoordinate->VTiling * 0.5f : 0.0f;
		Transform.Scale.X = TextureCoordinate->UTiling * (TextureCoordinate->UnMirrorU ? 0.5f : 1.0f);
		Transform.Scale.Y = TextureCoordinate->VTiling * (TextureCoordinate->UnMirrorV ? 0.5f : 1.0f);
		Transform.Rotation = 0;
		return true;
	}

	// TODO: add support for advanced expression tree (ex UMaterialExpressionTextureCoordinate -> UMaterialExpressionMultiply -> UMaterialExpressionAdd)

	return false;
}

void FGLTFMaterialUtilities::GetAllTextureCoordinateIndices(const UMaterialInterface* InMaterial, const FMaterialPropertyEx& InProperty, FGLTFIndexArray& OutTexCoords)
{
	FMaterialAnalysisResult Analysis;
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

void FGLTFMaterialUtilities::AnalyzeMaterialProperty(const UMaterialInterface* InMaterial, const FMaterialPropertyEx& InProperty, FMaterialAnalysisResult& OutAnalysis)
{
	if (GetInputForProperty(InMaterial, InProperty) == nullptr)
	{
		OutAnalysis = FMaterialAnalysisResult();
		return;
	}

	UMaterial* BaseMaterial = const_cast<UMaterial*>(InMaterial->GetMaterial());
	bool bRequiresPrimitiveData = false;

	// To extend and improve the analysis for glTF's specific use-case, we compile using FGLTFProxyMaterialCompiler
	BaseMaterial->AnalyzeMaterialCompilationInCallback([InProperty, BaseMaterial, &bRequiresPrimitiveData](FMaterialCompiler* Compiler)
	{
		FGLTFProxyMaterialCompiler ProxyCompiler(Compiler);

		if (InProperty.IsCustomOutput())
		{
			UMaterialExpressionCustomOutput* CustomOutput = GetCustomOutputByName(BaseMaterial, InProperty.CustomOutput.ToString());
			ProxyCompiler.SetMaterialProperty(MP_MAX, CustomOutput->GetShaderFrequency(), false);
			CustomOutput->Compile(&ProxyCompiler, 0);
		}
		else
		{
			ProxyCompiler.SetMaterialProperty(InProperty.Type, SF_NumFrequencies, false);
			BaseMaterial->CompileProperty(&ProxyCompiler, InProperty.Type);
		}

		bRequiresPrimitiveData = ProxyCompiler.UsesPrimitiveData();
	}, OutAnalysis);

	// Also make sure the analysis takes into account primitive data
	OutAnalysis.bRequiresVertexData |= bRequiresPrimitiveData;
}

FMaterialShadingModelField FGLTFMaterialUtilities::EvaluateShadingModelExpression(const UMaterialInterface* Material)
{
	FMaterialAnalysisResult Analysis;
	AnalyzeMaterialProperty(Material, MP_ShadingModel, Analysis);
	return Analysis.ShadingModels;
}

#endif

EMaterialShadingModel FGLTFMaterialUtilities::GetRichestShadingModel(const FMaterialShadingModelField& ShadingModels)
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

FString FGLTFMaterialUtilities::ShadingModelsToString(const FMaterialShadingModelField& ShadingModels)
{
	FString Result;

	for (uint32 Index = 0; Index < MSM_NUM; Index++)
	{
		const EMaterialShadingModel ShadingModel = static_cast<EMaterialShadingModel>(Index);
		if (ShadingModels.HasShadingModel(ShadingModel))
		{
			FString Name = FGLTFNameUtilities::GetName(ShadingModel);
			Result += Result.IsEmpty() ? Name : TEXT(", ") + Name;
		}
	}

	return Result;
}

bool FGLTFMaterialUtilities::NeedsMeshData(const UMaterialInterface* Material)
{
#if WITH_EDITOR
	if (Material != nullptr && !FGLTFProxyMaterialUtilities::IsProxyMaterial(Material))
	{
		// TODO: only analyze properties that will be needed for this specific material
		const TArray<FMaterialPropertyEx> Properties =
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
			FMaterialPropertyEx::ClearCoatBottomNormal,
			// TODO: add TransmittanceColor when supported
			// TODO: add Refraction when supported
			// TODO: add Specular when supported
		};

		bool bNeedsMeshData = false;
		FMaterialAnalysisResult Analysis;

		// TODO: optimize baking by separating need for vertex data and primitive data

		for (const FMaterialPropertyEx& Property: Properties)
		{
			AnalyzeMaterialProperty(Material, Property, Analysis);
			bNeedsMeshData |= Analysis.bRequiresVertexData;
		}

		return bNeedsMeshData;
	}
#endif

	return false;
}

bool FGLTFMaterialUtilities::NeedsMeshData(const TArray<const UMaterialInterface*>& Materials)
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
