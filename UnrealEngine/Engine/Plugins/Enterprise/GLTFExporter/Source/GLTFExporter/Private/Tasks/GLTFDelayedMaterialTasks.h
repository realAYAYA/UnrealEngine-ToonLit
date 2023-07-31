// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/GLTFDelayedTask.h"
#include "Builders/GLTFConvertBuilder.h"
#include "Converters/GLTFUVOverlapChecker.h"
#include "Converters/GLTFMeshData.h"
#include "Materials/GLTFProxyMaterialParameterInfo.h"
#include "Materials/Material.h"
#if WITH_EDITOR
#include "GLTFMaterialPropertyEx.h"
#endif

struct FGLTFPropertyBakeOutput;

class FGLTFDelayedMaterialTask : public FGLTFDelayedTask
{
public:

	FGLTFDelayedMaterialTask(FGLTFConvertBuilder& Builder, FGLTFUVOverlapChecker& UVOverlapChecker, const UMaterialInterface* Material, const FGLTFMeshData* MeshData, FGLTFIndexArray SectionIndices, FGLTFJsonMaterial* JsonMaterial)
		: FGLTFDelayedTask(EGLTFTaskPriority::Material)
		, Builder(Builder)
		, UVOverlapChecker(UVOverlapChecker)
		, Material(Material)
		, MeshData(MeshData)
		, SectionIndices(SectionIndices)
		, JsonMaterial(JsonMaterial)
	{
	}

	virtual FString GetName() override
	{
		return Material->GetName();
	}

	virtual void Process() override;

private:

	FGLTFConvertBuilder& Builder;
	FGLTFUVOverlapChecker& UVOverlapChecker;
	const UMaterialInterface* Material;
	const FGLTFMeshData* MeshData;
	const FGLTFIndexArray SectionIndices;
	FGLTFJsonMaterial* JsonMaterial;

	FString GetMaterialName() const;
	FString GetBakedTextureName(const FString& PropertyName) const;

	void GetProxyParameters(FGLTFJsonMaterial& OutMaterial) const;
	void GetProxyParameter(const TGLTFProxyMaterialParameterInfo<float>& ParameterInfo, float& OutValue) const;
	void GetProxyParameter(const TGLTFProxyMaterialParameterInfo<FLinearColor>& ParameterInfo, FGLTFJsonColor3& OutValue) const;
	void GetProxyParameter(const TGLTFProxyMaterialParameterInfo<FLinearColor>& ParameterInfo, FGLTFJsonColor4& OutValue) const;
	void GetProxyParameter(const FGLTFProxyMaterialTextureParameterInfo& ParameterInfo, FGLTFJsonTextureInfo& OutValue) const;

	EMaterialShadingModel GetShadingModel() const;
	void ConvertShadingModel(EGLTFJsonShadingModel& OutShadingModel) const;
	void ConvertAlphaMode(EGLTFJsonAlphaMode& OutAlphaMode, EGLTFJsonBlendMode& OutBlendMode) const;

#if WITH_EDITOR
	TSet<FGLTFMaterialPropertyEx> MeshDataBakedProperties;

	bool TryGetBaseColorAndOpacity(FGLTFJsonPBRMetallicRoughness& OutPBRParams, const FGLTFMaterialPropertyEx& BaseColorProperty, const FGLTFMaterialPropertyEx& OpacityProperty);
	bool TryGetMetallicAndRoughness(FGLTFJsonPBRMetallicRoughness& OutPBRParams, const FGLTFMaterialPropertyEx& MetallicProperty, const FGLTFMaterialPropertyEx& RoughnessProperty);
	bool TryGetClearCoatRoughness(FGLTFJsonClearCoatExtension& OutExtParams, const FGLTFMaterialPropertyEx& IntensityProperty, const FGLTFMaterialPropertyEx& RoughnessProperty);
	bool TryGetEmissive(FGLTFJsonMaterial& OutMaterial, const FGLTFMaterialPropertyEx& EmissiveProperty);

	bool IsPropertyNonDefault(const FGLTFMaterialPropertyEx& Property) const;
	bool TryGetConstantColor(FGLTFJsonColor3& OutValue, const FGLTFMaterialPropertyEx& Property) const;
	bool TryGetConstantColor(FGLTFJsonColor4& OutValue, const FGLTFMaterialPropertyEx& Property) const;
	bool TryGetConstantColor(FLinearColor& OutValue, const FGLTFMaterialPropertyEx& Property) const;
	bool TryGetConstantScalar(float& OutValue, const FGLTFMaterialPropertyEx& Property) const;

	bool TryGetSourceTexture(FGLTFJsonTextureInfo& OutTexInfo, const FGLTFMaterialPropertyEx& Property, const TArray<FLinearColor>& AllowedMasks = {}) const;
	bool TryGetSourceTexture(const UTexture2D*& OutTexture, int32& OutTexCoord, FGLTFJsonTextureTransform& OutTransform, const FGLTFMaterialPropertyEx& Property, const TArray<FLinearColor>& AllowedMasks = {}) const;

	bool TryGetBakedMaterialProperty(FGLTFJsonTextureInfo& OutTexInfo, FGLTFJsonColor3& OutConstant, const FGLTFMaterialPropertyEx& Property, const FString& PropertyName);
	bool TryGetBakedMaterialProperty(FGLTFJsonTextureInfo& OutTexInfo, FGLTFJsonColor4& OutConstant, const FGLTFMaterialPropertyEx& Property, const FString& PropertyName);
	bool TryGetBakedMaterialProperty(FGLTFJsonTextureInfo& OutTexInfo, float& OutConstant, const FGLTFMaterialPropertyEx& Property, const FString& PropertyName);
	bool TryGetBakedMaterialProperty(FGLTFJsonTextureInfo& OutTexInfo, const FGLTFMaterialPropertyEx& Property, const FString& PropertyName);

	FGLTFPropertyBakeOutput BakeMaterialProperty(const FGLTFMaterialPropertyEx& Property, int32& OutTexCoord);
	FGLTFPropertyBakeOutput BakeMaterialProperty(const FGLTFMaterialPropertyEx& Property, int32& OutTexCoord, const FIntPoint& TextureSize, bool bFillAlpha);

	bool StoreBakedPropertyTexture(FGLTFJsonTextureInfo& OutTexInfo, FGLTFPropertyBakeOutput& PropertyBakeOutput, const FString& PropertyName) const;

	static EGLTFMaterialPropertyGroup GetPropertyGroup(const FGLTFMaterialPropertyEx& Property);

	template <typename CallbackType>
	static void CombinePixels(const TArray<FColor>& FirstPixels, const TArray<FColor>& SecondPixels, TArray<FColor>& OutPixels, CallbackType Callback);

#endif
};
