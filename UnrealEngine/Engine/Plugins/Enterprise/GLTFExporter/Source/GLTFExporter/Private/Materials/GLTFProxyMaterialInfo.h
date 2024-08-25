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
	static const TGLTFProxyMaterialParameterInfo<float> EmissiveStrength;

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
	
	static const TGLTFProxyMaterialParameterInfo<float> SpecularFactor;
	static const FGLTFProxyMaterialTextureParameterInfo SpecularTexture; //Only using Alpha Channel

	static const TGLTFProxyMaterialParameterInfo<float> IOR;

	static const TGLTFProxyMaterialParameterInfo<FLinearColor> SheenColorFactor;
	static const FGLTFProxyMaterialTextureParameterInfo        SheenColorTexture; //RGB
	static const TGLTFProxyMaterialParameterInfo<float>        SheenRoughnessFactor;
	static const FGLTFProxyMaterialTextureParameterInfo        SheenRoughnessTexture; //A

	static const TGLTFProxyMaterialParameterInfo<float> TransmissionFactor;
	static const FGLTFProxyMaterialTextureParameterInfo TransmissionTexture; //Only using Red Channel
};
