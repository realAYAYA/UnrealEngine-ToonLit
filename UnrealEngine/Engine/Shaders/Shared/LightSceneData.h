// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef __cplusplus
#include "HLSLTypeAliases.h"

namespace UE::HLSL
{
#endif
#ifndef __cplusplus //HLSL
#include "/Engine/Private/LargeWorldCoordinates.ush"
#endif


/**
 * Has a 1:1 mapping with FLightRenderParameters, but unlike FLightShaderParameters, this is view-independent
 */
struct FLightSceneData
{
	// Position of the light in world space.
	FDFVector3 WorldPosition;

	// 1 / light's falloff radius from Position.
	float InvRadius;

	// The exponent for the falloff of the light intensity from the distance.
	float FalloffExponent;

	// Color of the light.
	float4 Color;

	// Direction of the light if applies.
	float3 Direction;

	// Factor to applies on the specular.
	float SpecularScale;

	// One tangent of the light if applies.
	// Note: BiTangent is on purpose not stored for memory optimisation purposes.
	float3 Tangent;

	// Radius of the point light.
	float SourceRadius;

	// Dimensions of the light, for spot light, but also
	float2 SpotAngles;

	// Radius of the soft source.
	float SoftSourceRadius;

	// Other dimensions of the light source for rect light specifically.
	float SourceLength;

	// Barn door angle for rect light
	float RectLightBarnCosAngle;

	// Barn door length for rect light
	float RectLightBarnLength;

	// Rect. light atlas transformation
	float2 RectLightAtlasUVOffset;
	float2 RectLightAtlasUVScale;
	float RectLightAtlasMaxLevel;

	float InverseExposureBlend;

	// could pack IESAtlasIndex with other data in the future since it doesn't require 32 bits
	// FLocalLightData packs it in 16 bits (see UnpackLigthIESAtlasIndex(...))
	// could probably go down to 8 bits (with some logic in GIESTextureManager to warn about overflow)
	float IESAtlasIndex;

	// Extra fields
	uint LightTypeAndShadowMapChannelMaskPacked;

	float2 Padding;
};

#ifdef __cplusplus
} // namespace
using FLightSceneData = UE::HLSL::FLightSceneData;
#endif
