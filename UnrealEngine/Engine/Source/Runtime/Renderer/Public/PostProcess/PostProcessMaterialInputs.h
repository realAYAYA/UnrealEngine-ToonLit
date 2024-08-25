// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneTexturesConfig.h"
#include "ScreenPass.h"

struct FSceneWithoutWaterTextures;

const uint32 kPostProcessMaterialInputCountMax = 5;
const uint32 kPathTracingPostProcessMaterialInputCountMax = 5;

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

enum class EPathTracingPostProcessMaterialInput : uint32
{
	Radiance = 0,
	DenoisedRadiance = 1,
	Albedo = 2,
	Normal = 3,
	Variance = 4
};

struct FPostProcessMaterialInputs
{
	inline void SetInput(FRDGBuilder& GraphBuilder, EPostProcessMaterialInput Input, FScreenPassTexture Texture)
	{
		SetInput(Input, FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, Texture));
	}

	inline void SetInput(EPostProcessMaterialInput Input, FScreenPassTextureSlice Texture)
	{
		Textures[(uint32)Input] = Texture;
	}

	inline FScreenPassTextureSlice GetInput(EPostProcessMaterialInput Input) const
	{
		return Textures[(uint32)Input];
	}

	inline void SetPathTracingInput(EPathTracingPostProcessMaterialInput Input, FScreenPassTexture Texture)
	{
		PathTracingTextures[(uint32)Input] = Texture;
	}

	inline FScreenPassTexture GetPathTracingInput(EPathTracingPostProcessMaterialInput Input) const
	{
		return PathTracingTextures[(uint32)Input];
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
		const FScreenPassTextureSlice Texture = GetInput(EPostProcessMaterialInput::SceneColor);
		check(Texture.IsValid());
	}

	/**
	* A helper function that extracts the right scene color texture, untouched, to be used further in post processing.
	*/
	inline FScreenPassTexture ReturnUntouchedSceneColorForPostProcessing(FRDGBuilder& GraphBuilder) const
	{
		if (OverrideOutput.IsValid())
		{
			return OverrideOutput;
		}
		else
		{
			/** We don't want to modify scene texture in any way. We just want it to be passed back onto the next stage. */
			FScreenPassTextureSlice SceneTexture = const_cast<FScreenPassTextureSlice&>(Textures[(uint32)EPostProcessMaterialInput::SceneColor]);
			return FScreenPassTexture::CopyFromSlice(GraphBuilder, SceneTexture);
		}
	}

	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	/** Array of input textures bound to the material. The first element represents the output from
	 *  the previous post process and is required. All other inputs are optional.
	 */
	TStaticArray<FScreenPassTextureSlice, kPostProcessMaterialInputCountMax> Textures;

	/**
	*	Array of input textures bound to the material from path tracing. All inputs are optional
	*/
	TStaticArray<FScreenPassTexture, kPathTracingPostProcessMaterialInputCountMax> PathTracingTextures;

	/** The output texture format to use if a new texture is created. Uses the input format if left unknown. */
	EPixelFormat OutputFormat = PF_Unknown;

	/** Whether or not the stencil test must be done in the pixel shader rather than rasterizer state. */
	bool bManualStencilTest = false;

	/** Custom depth/stencil used for stencil operations. */
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

class UMaterialInterface;

FScreenPassTexture RENDERER_API AddPostProcessMaterialPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs,
	const UMaterialInterface* MaterialInterface);
