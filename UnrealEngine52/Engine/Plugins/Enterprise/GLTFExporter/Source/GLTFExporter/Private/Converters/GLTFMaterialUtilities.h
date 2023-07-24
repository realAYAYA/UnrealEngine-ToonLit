// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFContainerBuilder.h"
#include "Converters/GLTFSharedArray.h"
#include "MaterialExpressionIO.h"

struct FMaterialAnalysisResult;
struct FMaterialPropertyEx;
class UMaterialExpressionTextureSample;
class UMaterialInstanceConstant;
class UMaterialInterface;
class UMaterialExpressionCustomOutput;

struct FGLTFPropertyBakeOutput
{
	FGLTFPropertyBakeOutput(const FMaterialPropertyEx& Property, EPixelFormat PixelFormat, const TGLTFSharedArray<FColor>& Pixels, FIntPoint Size, float EmissiveScale, bool bIsSRGB)
		: Property(Property), PixelFormat(PixelFormat), Pixels(Pixels), Size(Size), EmissiveScale(EmissiveScale), bIsSRGB(bIsSRGB), bIsConstant(false)
	{}

	const FMaterialPropertyEx& Property;
	EPixelFormat PixelFormat;
	TGLTFSharedArray<FColor> Pixels;
	FIntPoint Size;
	float EmissiveScale;
	bool bIsSRGB;
	bool bIsConstant;
	FLinearColor ConstantValue;
};

struct FGLTFMaterialUtilities
{
	static UMaterialInterface* GetDefaultMaterial();

	static bool IsClearCoatBottomNormalEnabled();

#if WITH_EDITOR
	static bool IsNormalMap(const FMaterialPropertyEx& Property);
	static bool IsSRGB(const FMaterialPropertyEx& Property);

	static FGuid GetAttributeID(const FMaterialPropertyEx& Property);
	static FGuid GetAttributeIDChecked(const FMaterialPropertyEx& Property);

	static FVector4f GetPropertyDefaultValue(const FMaterialPropertyEx& Property);
	static FVector4f GetPropertyMask(const FMaterialPropertyEx& Property);

	static const FExpressionInput* GetInputForProperty(const UMaterialInterface* Material, const FMaterialPropertyEx& Property);

	template <class InputType>
	static const FMaterialInput<InputType>* GetInputForProperty(const UMaterialInterface* Material, const FMaterialPropertyEx& Property)
	{
		const FExpressionInput* ExpressionInput = GetInputForProperty(Material, Property);
		return static_cast<const FMaterialInput<InputType>*>(ExpressionInput);
	}

	static UMaterialExpressionCustomOutput* GetCustomOutputByName(const UMaterialInterface* Material, const FString& FunctionName);

	static FGLTFPropertyBakeOutput BakeMaterialProperty(const FIntPoint& OutputSize, const FMaterialPropertyEx& Property, const UMaterialInterface* Material, int32 TexCoord, const FGLTFMeshData* MeshData = nullptr, const FGLTFIndexArray& MeshSectionIndices = {}, bool bFillAlpha = true, bool bAdjustNormalmaps = true);

	static FGLTFJsonTexture* AddTexture(FGLTFConvertBuilder& Builder, TGLTFSharedArray<FColor>& Pixels, const FIntPoint& TextureSize, bool bIgnoreAlpha, bool bIsNormalMap, const FString& TextureName, TextureAddress TextureAddress, TextureFilter TextureFilter);

	static FLinearColor GetMask(const FExpressionInput& ExpressionInput);
	static uint32 GetMaskComponentCount(const FExpressionInput& ExpressionInput);

	static bool TryGetTextureCoordinateIndex(const UMaterialExpressionTextureSample* TextureSampler, int32& TexCoord, FGLTFJsonTextureTransform& Transform);
	static void GetAllTextureCoordinateIndices(const UMaterialInterface* InMaterial, const FMaterialPropertyEx& InProperty, FGLTFIndexArray& OutTexCoords);

	static void AnalyzeMaterialProperty(const UMaterialInterface* Material, const FMaterialPropertyEx& InProperty, FMaterialAnalysisResult& OutAnalysis);

	static FMaterialShadingModelField EvaluateShadingModelExpression(const UMaterialInterface* Material);
#endif

	static EMaterialShadingModel GetRichestShadingModel(const FMaterialShadingModelField& ShadingModels);
	static FString ShadingModelsToString(const FMaterialShadingModelField& ShadingModels);

	static bool NeedsMeshData(const UMaterialInterface* Material);
	static bool NeedsMeshData(const TArray<const UMaterialInterface*>& Materials);
};
