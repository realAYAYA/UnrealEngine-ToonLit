// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"
#include "DataDrivenShaderPlatformInfo.h"

//UE_ENABLE_DEBUG_DRAWING i.e. !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR
//Only available in Debug/Development/Editor builds due to current use cases, but can be extended in future
#if UE_ENABLE_DEBUG_DRAWING

/** Base class for a global pixel shader which renders primitives (outlines, helpers, etc). */
class FCompositePrimitiveShaderBase : public FGlobalShader
{
public:
	static const uint32 kMSAASampleCountMax = 8;

	class FSampleCountDimension : SHADER_PERMUTATION_RANGE_INT("MSAA_SAMPLE_COUNT", 1, kMSAASampleCountMax + 1);
	using FPermutationDomain = TShaderPermutationDomain<FSampleCountDimension>;

	static bool ShouldCompilePermutation(const FPermutationDomain& PermutationVector, const EShaderPlatform Platform)
	{
		const int32 SampleCount = PermutationVector.Get<FSampleCountDimension>();

		// Only use permutations with valid MSAA sample counts.
		if (!FMath::IsPowerOfTwo(SampleCount))
		{
			return false;
		}

		return IsPCPlatform(Platform);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		return ShouldCompilePermutation(PermutationVector, Parameters.Platform);
	}

	FCompositePrimitiveShaderBase() = default;
	FCompositePrimitiveShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

struct FCompositePrimitiveInputs
{
	enum class EBasePassType : uint32
	{
		Deferred,
		Mobile,
		MAX
	};
	// [Required] The type of base pass to use for rendering editor primitives.
	EBasePassType BasePassType = EBasePassType::MAX;

	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	// [Required] The scene color to composite with editor primitives.
	FScreenPassTexture SceneColor;

	// [Required] The scene depth to composite with editor primitives.
	FScreenPassTexture SceneDepth;

	bool bUseMetalMSAAHDRDecode = false;
};

// Constructs a new view suitable for rendering debug primitives.
const FViewInfo* CreateCompositePrimitiveView(const FViewInfo& ParentView, FIntRect ViewRect, uint32 NumMSAASamples);

FRDGTextureRef CreateCompositeDepthTexture(FRDGBuilder& GraphBuilder, FIntPoint Extent, uint32 NumMSAASamples);


void TemporalUpscaleDepthPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FScreenPassTexture& InSceneColor,
	FScreenPassTexture& InOutSceneDepth,
	FVector2f& SceneDepthJitter);

void PopulateDepthPass(FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FScreenPassTexture& InSceneColor, 
	const FScreenPassTexture& InSceneDepth, 
	FRDGTextureRef OutPopColor, 
	FRDGTextureRef OutPopDepth, 
	const FVector2f& SceneDepthJitter,
	uint32 NumMSAASamples,
	bool bForceDrawColor = false,
	bool bUseMetalPlatformHDRDecode = false);

#endif //#if UE_ENABLE_DEBUG_DRAWING