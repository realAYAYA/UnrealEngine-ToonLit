// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"
#include "Strata/Strata.h"

class UMaterialInterface;
struct FSceneWithoutWaterTextures;

const uint32 kPostProcessMaterialInputCountMax = 5;

using FPostProcessMaterialChain = TArray<const UMaterialInterface*, TInlineAllocator<10>>;

FPostProcessMaterialChain GetPostProcessMaterialChain(const FViewInfo& View, EBlendableLocation Location);

/** Named post process material slots. Inputs are aliased and have different semantics
 *  based on the post process material blend point, which is documented with the input.
 */
enum class EPostProcessMaterialInput : uint32
{
	// Always Active. Color from the previous stage of the post process chain.
	SceneColor = 0,

	// Always Active.
	SeparateTranslucency = 1,

	// Replace Tonemap Only. Half resolution combined bloom input.
	CombinedBloom = 2,

	// Buffer Visualization Only.
	PreTonemapHDRColor = 2,
	PostTonemapHDRColor = 3,

	// Active if separate velocity pass is used--i.e. not part of base pass; Not active during Replace Tonemap.
	Velocity = 4
};

BEGIN_SHADER_PARAMETER_STRUCT(FPostProcessMaterialParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PostProcessOutput)
	SHADER_PARAMETER_STRUCT_ARRAY(FScreenPassTextureInput, PostProcessInput, [kPostProcessMaterialInputCountMax])
	SHADER_PARAMETER_SAMPLER(SamplerState, PostProcessInput_BilinearSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EyeAdaptationTexture)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, EyeAdaptationBuffer)
	SHADER_PARAMETER(uint32, bMetalMSAAHDRDecode)
	SHADER_PARAMETER(uint32, bSceneDepthWithoutWaterTextureAvailable)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthWithoutSingleLayerWaterTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthWithoutSingleLayerWaterSampler)
	SHADER_PARAMETER(FVector4f, SceneWithoutSingleLayerWaterMinMaxUV)
	SHADER_PARAMETER(FVector2f, SceneWithoutSingleLayerWaterTextureSize)
	SHADER_PARAMETER(FVector2f, SceneWithoutSingleLayerWaterInvTextureSize)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

struct FPostProcessMaterialInputs
{
	inline void SetInput(EPostProcessMaterialInput Input, FScreenPassTexture Texture)
	{
		Textures[(uint32)Input] = Texture;
	}

	inline FScreenPassTexture GetInput(EPostProcessMaterialInput Input) const
	{
		return Textures[(uint32)Input];
	}

	inline void Validate() const
	{
		ValidateInputExists(EPostProcessMaterialInput::SceneColor);
		ValidateInputExists(EPostProcessMaterialInput::SeparateTranslucency);

		// Either override output format is valid or the override output texture is; not both.
		if (OutputFormat != PF_Unknown)
		{
			check(OverrideOutput.Texture == nullptr);
		}
		if (OverrideOutput.Texture)
		{
			check(OutputFormat == PF_Unknown);
		}

		check(SceneTextures.SceneTextures || SceneTextures.MobileSceneTextures);
	}

	inline void ValidateInputExists(EPostProcessMaterialInput Input) const
	{
		const FScreenPassTexture Texture = GetInput(EPostProcessMaterialInput::SceneColor);
		check(Texture.IsValid());
	}

	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	/** Array of input textures bound to the material. The first element represents the output from
	 *  the previous post process and is required. All other inputs are optional.
	 */
	TStaticArray<FScreenPassTexture, kPostProcessMaterialInputCountMax> Textures;

	/** The output texture format to use if a new texture is created. Uses the input format if left unknown. */
	EPixelFormat OutputFormat = PF_Unknown;

	/** Custom stencil texture used for stencil operations. */
	FRDGTextureRef CustomDepthTexture = nullptr;

	/** The uniform buffer containing all scene textures. */
	FSceneTextureShaderParameters SceneTextures;

	/** Depth and color textures of the scene without single layer water. May be nullptr if not available. */
	const FSceneWithoutWaterTextures* SceneWithoutWaterTextures = nullptr;

	/** Allows (but doesn't guarantee) an optimization where, if possible, the scene color input is reused as
	 *  the output. This can elide a copy in certain circumstances; for example, when the scene color input isn't
	 *  actually used by the post process material and no special depth-stencil / blend composition is required.
	 *  Set this to false when you need to guarantee creation of a dedicated output texture.
	 */
	bool bAllowSceneColorInputAsOutput = true;

	bool bMetalMSAAHDRDecode = false;
};

FScreenPassTexture AddPostProcessMaterialPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FPostProcessMaterialInputs& Inputs,
	const UMaterialInterface* MaterialInterface);

FScreenPassTexture AddPostProcessMaterialChain(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FPostProcessMaterialInputs& Inputs,
	const FPostProcessMaterialChain& MaterialChain);

struct FHighResolutionScreenshotMaskInputs
{
	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	FScreenPassTexture SceneColor;

	FSceneTextureShaderParameters SceneTextures;

	UMaterialInterface* Material = nullptr;
	UMaterialInterface* MaskMaterial = nullptr;
	UMaterialInterface* CaptureRegionMaterial = nullptr;
};

bool IsHighResolutionScreenshotMaskEnabled(const FViewInfo& View);

FScreenPassTexture AddHighResolutionScreenshotMaskPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FHighResolutionScreenshotMaskInputs& Inputs);