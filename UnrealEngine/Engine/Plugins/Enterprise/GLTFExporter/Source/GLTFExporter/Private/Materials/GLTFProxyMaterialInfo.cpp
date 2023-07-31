// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/GLTFProxyMaterialInfo.h"

const FGLTFProxyMaterialTextureParameterInfo FGLTFProxyMaterialInfo::BaseColor = { TEXT("Base Color") };
const TGLTFProxyMaterialParameterInfo<FLinearColor> FGLTFProxyMaterialInfo::BaseColorFactor = { TEXT("Base Color Factor") };

const FGLTFProxyMaterialTextureParameterInfo FGLTFProxyMaterialInfo::Emissive = { TEXT("Emissive") };
const TGLTFProxyMaterialParameterInfo<FLinearColor> FGLTFProxyMaterialInfo::EmissiveFactor = { TEXT("Emissive Factor") };

const FGLTFProxyMaterialTextureParameterInfo FGLTFProxyMaterialInfo::MetallicRoughness = { TEXT("Metallic Roughness") };
const TGLTFProxyMaterialParameterInfo<float> FGLTFProxyMaterialInfo::MetallicFactor = { TEXT("Metallic Factor") };
const TGLTFProxyMaterialParameterInfo<float> FGLTFProxyMaterialInfo::RoughnessFactor = { TEXT("Roughness Factor") };

const FGLTFProxyMaterialTextureParameterInfo FGLTFProxyMaterialInfo::Normal = { TEXT("Normal") };
const TGLTFProxyMaterialParameterInfo<float> FGLTFProxyMaterialInfo::NormalScale = { TEXT("Normal Scale") };

const FGLTFProxyMaterialTextureParameterInfo FGLTFProxyMaterialInfo::Occlusion = { TEXT("Occlusion") };
const TGLTFProxyMaterialParameterInfo<float> FGLTFProxyMaterialInfo::OcclusionStrength = { TEXT("Occlusion Strength") };

const FGLTFProxyMaterialTextureParameterInfo FGLTFProxyMaterialInfo::ClearCoat = { TEXT("Clear Coat") };
const TGLTFProxyMaterialParameterInfo<float> FGLTFProxyMaterialInfo::ClearCoatFactor = { TEXT("Clear Coat Factor") };

const FGLTFProxyMaterialTextureParameterInfo FGLTFProxyMaterialInfo::ClearCoatRoughness = { TEXT("Clear Coat Roughness") };
const TGLTFProxyMaterialParameterInfo<float> FGLTFProxyMaterialInfo::ClearCoatRoughnessFactor = { TEXT("Clear Coat Roughness Factor") };

const FGLTFProxyMaterialTextureParameterInfo FGLTFProxyMaterialInfo::ClearCoatNormal = { TEXT("Clear Coat Normal") };
const TGLTFProxyMaterialParameterInfo<float> FGLTFProxyMaterialInfo::ClearCoatNormalScale = { TEXT("Clear Coat Normal Scale") };
