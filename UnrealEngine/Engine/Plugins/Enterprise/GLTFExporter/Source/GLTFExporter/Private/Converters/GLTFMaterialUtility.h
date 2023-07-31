// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFContainerBuilder.h"
#include "Converters/GLTFSharedArray.h"
#include "MaterialExpressionIO.h"

struct FGLTFMaterialAnalysis;
struct FGLTFMaterialPropertyEx;
class UMaterialExpressionTextureSample;
class UMaterialInstanceConstant;
class UMaterialInterface;

struct FGLTFPropertyBakeOutput
{
	FGLTFPropertyBakeOutput(const FGLTFMaterialPropertyEx& Property, EPixelFormat PixelFormat, const TGLTFSharedArray<FColor>& Pixels, FIntPoint Size, float EmissiveScale, bool bIsSRGB)
		: Property(Property), PixelFormat(PixelFormat), Pixels(Pixels), Size(Size), EmissiveScale(EmissiveScale), bIsSRGB(bIsSRGB), bIsConstant(false)
	{}

	const FGLTFMaterialPropertyEx& Property;
	EPixelFormat PixelFormat;
	TGLTFSharedArray<FColor> Pixels;
	FIntPoint Size;
	float EmissiveScale;
	bool bIsSRGB;
	bool bIsConstant;
	FLinearColor ConstantValue;
};

struct FGLTFMaterialUtility
{
	static UMaterialInterface* GetDefaultMaterial();

#if WITH_EDITOR
	static bool IsNormalMap(const FGLTFMaterialPropertyEx& Property);
	static bool IsSRGB(const FGLTFMaterialPropertyEx& Property);

	static FGuid GetAttributeID(const FGLTFMaterialPropertyEx& Property);
	static FGuid GetAttributeIDChecked(const FGLTFMaterialPropertyEx& Property);

	static FVector4f GetPropertyDefaultValue(const FGLTFMaterialPropertyEx& Property);
	static FVector4f GetPropertyMask(const FGLTFMaterialPropertyEx& Property);

	static const FExpressionInput* GetInputForProperty(const UMaterialInterface* Material, const FGLTFMaterialPropertyEx& Property);

	template <class InputType>
	static const FMaterialInput<InputType>* GetInputForProperty(const UMaterialInterface* Material, const FGLTFMaterialPropertyEx& Property)
	{
		const FExpressionInput* ExpressionInput = GetInputForProperty(Material, Property);
		return static_cast<const FMaterialInput<InputType>*>(ExpressionInput);
	}

	static const UMaterialExpressionCustomOutput* GetCustomOutputByName(const UMaterialInterface* Material, const FString& Name);

	static FGLTFPropertyBakeOutput BakeMaterialProperty(const FIntPoint& OutputSize, const FGLTFMaterialPropertyEx& Property, const UMaterialInterface* Material, int32 TexCoord, const FGLTFMeshData* MeshData = nullptr, const FGLTFIndexArray& MeshSectionIndices = {}, bool bFillAlpha = true, bool bAdjustNormalmaps = true);

	static FGLTFJsonTexture* AddTexture(FGLTFConvertBuilder& Builder, TGLTFSharedArray<FColor>& Pixels, const FIntPoint& TextureSize, bool bIgnoreAlpha, bool bIsNormalMap, const FString& TextureName, EGLTFJsonTextureFilter MinFilter, EGLTFJsonTextureFilter MagFilter, EGLTFJsonTextureWrap WrapS, EGLTFJsonTextureWrap WrapT);

	static FLinearColor GetMask(const FExpressionInput& ExpressionInput);
	static uint32 GetMaskComponentCount(const FExpressionInput& ExpressionInput);

	static bool TryGetTextureCoordinateIndex(const UMaterialExpressionTextureSample* TextureSampler, int32& TexCoord, FGLTFJsonTextureTransform& Transform);
	static void GetAllTextureCoordinateIndices(const UMaterialInterface* InMaterial, const FGLTFMaterialPropertyEx& InProperty, FGLTFIndexArray& OutTexCoords);

	static void AnalyzeMaterialProperty(const UMaterialInterface* Material, const FGLTFMaterialPropertyEx& InProperty, FGLTFMaterialAnalysis& OutAnalysis);

	static FMaterialShadingModelField EvaluateShadingModelExpression(const UMaterialInterface* Material);
#endif

	static EMaterialShadingModel GetRichestShadingModel(const FMaterialShadingModelField& ShadingModels);
	static FString ShadingModelsToString(const FMaterialShadingModelField& ShadingModels);

	static bool NeedsMeshData(const UMaterialInterface* Material);
	static bool NeedsMeshData(const TArray<const UMaterialInterface*>& Materials);
};
