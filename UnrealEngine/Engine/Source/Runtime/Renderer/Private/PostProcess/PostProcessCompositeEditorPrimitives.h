// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"

#if WITH_EDITOR

/** Base class for a global pixel shader which renders editor primitives (outlines, helpers, etc). */
class FEditorPrimitiveShader : public FGlobalShader
{
public:
	static const uint32 kEditorMSAASampleCountMax = 8;

	class FSampleCountDimension : SHADER_PERMUTATION_RANGE_INT("MSAA_SAMPLE_COUNT", 1, kEditorMSAASampleCountMax + 1);
	using FPermutationDomain = TShaderPermutationDomain<FSampleCountDimension>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 SampleCount = PermutationVector.Get<FSampleCountDimension>();

		// Only use permutations with valid MSAA sample counts.
		if (!FMath::IsPowerOfTwo(SampleCount))
		{
			return false;
		}

		// Only PC platforms render editor primitives.
		return IsPCPlatform(Parameters.Platform);
	}

	FEditorPrimitiveShader() = default;
	FEditorPrimitiveShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

// Constructs a new view suitable for rendering editor primitives (outlines, helpers, etc).
const FViewInfo* CreateEditorPrimitiveView(const FViewInfo& ParentView, FIntRect ViewRect, uint32 NumSamples);

struct FEditorPrimitiveInputs
{
	enum class EBasePassType : uint32
	{
		Deferred,
		Mobile,
		MAX
	};

	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	// [Required] The scene color to composite with editor primitives.
	FScreenPassTexture SceneColor;

	// [Required] The scene depth to composite with editor primitives.
	FScreenPassTexture SceneDepth;

	// [Required] The type of base pass to use for rendering editor primitives.
	EBasePassType BasePassType = EBasePassType::MAX;
};

FScreenPassTexture AddEditorPrimitivePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FEditorPrimitiveInputs& Inputs, FInstanceCullingManager& InstanceCullingManager);

#endif
