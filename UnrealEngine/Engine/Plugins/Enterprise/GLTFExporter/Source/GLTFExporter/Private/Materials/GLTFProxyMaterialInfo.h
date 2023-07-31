// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/GLTFProxyMaterialParameterInfo.h"

class GLTFEXPORTER_API FGLTFProxyMaterialInfo
{
public:

	static const FGLTFProxyMaterialTextureParameterInfo BaseColor;
	static const TGLTFProxyMaterialParameterInfo<FLinearColor> BaseColorFactor;

	static const FGLTFProxyMaterialTextureParameterInfo Emissive;
	static const TGLTFProxyMaterialParameterInfo<FLinearColor> EmissiveFactor;

	static const FGLTFProxyMaterialTextureParameterInfo MetallicRoughness;
	static const TGLTFProxyMaterialParameterInfo<float> MetallicFactor;
	static const TGLTFProxyMaterialParameterInfo<float> RoughnessFactor;

	static const FGLTFProxyMaterialTextureParameterInfo Normal;
	static const TGLTFProxyMaterialParameterInfo<float> NormalScale;

	static const FGLTFProxyMaterialTextureParameterInfo Occlusion;
	static const TGLTFProxyMaterialParameterInfo<float> OcclusionStrength;

	static const FGLTFProxyMaterialTextureParameterInfo ClearCoat;
	static const TGLTFProxyMaterialParameterInfo<float> ClearCoatFactor;

	static const FGLTFProxyMaterialTextureParameterInfo ClearCoatRoughness;
	static const TGLTFProxyMaterialParameterInfo<float> ClearCoatRoughnessFactor;

	static const FGLTFProxyMaterialTextureParameterInfo ClearCoatNormal;
	static const TGLTFProxyMaterialParameterInfo<float> ClearCoatNormalScale;
};
