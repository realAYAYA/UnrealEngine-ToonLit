// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "PostProcess/PostProcessMotionBlur.h"

struct FTemporalAAHistory;
struct FTranslucencyPassResources;


/** Configuration of the main temporal AA pass. */
enum class EMainTAAPassConfig : uint8
{
	// TAA is disabled.
	Disabled,

	// Uses old UE4's Temporal AA maintained for Gen4 consoles
	TAA,

	// Uses Temporal Super Resolution
	TSR,

	// Uses third party View.Family->GetTemporalUpscalerInterface()
	ThirdParty,
};


/** List of TAA configurations. */
enum class ETAAPassConfig
{
	// Permutations for main scene color TAA.
	Main,
	MainUpsampling,
	MainSuperSampling,

	// Permutation for SSR noise accumulation.
	ScreenSpaceReflections,
	
	// Permutation for light shaft noise accumulation.
	LightShaft,

	// Permutation for DOF that handle Coc.
	DiaphragmDOF,
	DiaphragmDOFUpsampling,
	
	// Permutation for hair.
	Hair,

	MAX
};

static FORCEINLINE bool IsTAAUpsamplingConfig(ETAAPassConfig Pass)
{
	return Pass == ETAAPassConfig::MainUpsampling || Pass == ETAAPassConfig::DiaphragmDOFUpsampling || Pass == ETAAPassConfig::MainSuperSampling;
}

static FORCEINLINE bool IsMainTAAConfig(ETAAPassConfig Pass)
{
	return Pass == ETAAPassConfig::Main || Pass == ETAAPassConfig::MainUpsampling || Pass == ETAAPassConfig::MainSuperSampling;
}

static FORCEINLINE bool IsDOFTAAConfig(ETAAPassConfig Pass)
{
	return Pass == ETAAPassConfig::DiaphragmDOF || Pass == ETAAPassConfig::DiaphragmDOFUpsampling;
}

/** GPU Output of the TAA pass. */
struct FTAAOutputs
{
	// Anti aliased scene color.
	// Can have alpha channel, or CoC for DOF.
	FRDGTexture* SceneColor = nullptr;

	// Optional information that get anti aliased, such as separate CoC for DOF.
	FRDGTexture* SceneMetadata = nullptr;

	// Optional scene color output at half the resolution.
	FRDGTexture* DownsampledSceneColor = nullptr;
};

/** Quality of TAA. */
enum class ETAAQuality : uint8
{
	Low,
	Medium,
	High,
	MAX
};

/** Configuration of TAA. */
struct FTAAPassParameters
{
	// TAA pass to run.
	ETAAPassConfig Pass = ETAAPassConfig::Main;

	// Whether to use the faster shader permutation.
	ETAAQuality Quality = ETAAQuality::High;

	// Whether output texture should be render targetable.
	bool bOutputRenderTargetable = false;

	// Whether downsampled (box filtered, half resolution) frame should be written out.
	bool bDownsample = false;
	EPixelFormat DownsampleOverrideFormat = PF_Unknown;

	// Viewport rectangle of the input and output of TAA at ResolutionDivisor == 1.
	FIntRect InputViewRect;
	FIntRect OutputViewRect;

	// Resolution divisor.
	int32 ResolutionDivisor = 1;

	// Full resolution depth and velocity textures to reproject the history.
	FRDGTexture* SceneDepthTexture = nullptr;
	FRDGTexture* SceneVelocityTexture = nullptr;

	// Anti aliased scene color.
	// Can have alpha channel, or CoC for DOF.
	FRDGTexture* SceneColorInput = nullptr;

	// Optional information that get anti aliased, such as separate CoC for DOF.
	FRDGTexture* SceneMetadataInput = nullptr;


	FTAAPassParameters(const FViewInfo& View)
		: InputViewRect(View.ViewRect)
		, OutputViewRect(View.ViewRect)
	{ }


	// Customizes the view rectangles for input and output.
	FORCEINLINE void SetupViewRect(const FViewInfo& View, int32 InResolutionDivisor = 1)
	{
		ResolutionDivisor = InResolutionDivisor;

		InputViewRect = View.ViewRect;

		// When upsampling, always upsampling to top left corner to reuse same RT as before upsampling.
		if (IsTAAUpsamplingConfig(Pass))
		{
			OutputViewRect.Min = FIntPoint(0, 0);
			OutputViewRect.Max =  View.GetSecondaryViewRectSize();
		}
		else
		{
			OutputViewRect = InputViewRect;
		}
	}

	// Shifts input and output view rect to top left corner
	FORCEINLINE void TopLeftCornerViewRects()
	{
		InputViewRect.Max -= InputViewRect.Min;
		InputViewRect.Min = FIntPoint::ZeroValue;
		OutputViewRect.Max -= OutputViewRect.Min;
		OutputViewRect.Min = FIntPoint::ZeroValue;
	}
	
	/** Returns the texture resolution that will be output. */
	FIntPoint GetOutputExtent() const;

	/** Validate the settings of TAA, to make sure there is no issue. */
	bool Validate() const;
};

/** Temporal AA pass which emits a filtered scene color and new history. */
extern RENDERER_API FTAAOutputs AddTemporalAAPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FTAAPassParameters& Inputs,
	const FTemporalAAHistory& InputHistory,
	FTemporalAAHistory* OutputHistory);

extern RENDERER_API FScreenPassTexture AddTSRComputeMoireLuma(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FScreenPassTexture SceneColor);

/** Interface for the main temporal upscaling algorithm. */
class RENDERER_API ITemporalUpscaler : public ISceneViewFamilyExtention
{
public:

	struct FPassInputs
	{
		bool bGenerateSceneColorHalfRes = false;
		bool bGenerateSceneColorQuarterRes = false;
		bool bGenerateOutputMip1 = false;
		bool bGenerateVelocityFlattenTextures = false;
		EPixelFormat DownsampleOverrideFormat;
		FRDGTextureRef SceneColorTexture = nullptr;
		FRDGTextureRef SceneDepthTexture = nullptr;
		FRDGTextureRef SceneVelocityTexture = nullptr;
		FTranslucencyPassResources PostDOFTranslucencyResources;
		FScreenPassTexture MoireInputTexture;
	};

	struct FOutputs
	{
		FScreenPassTexture FullRes;
		FScreenPassTexture HalfRes;
		FScreenPassTexture QuarterRes;
		FVelocityFlattenTextures VelocityFlattenTextures;
	};

	virtual ~ITemporalUpscaler() {};

	virtual const TCHAR* GetDebugName() const = 0;

	///** Temporal AA helper method which performs filtering on the main pass scene color. Supports upsampled history and,
	// *  if requested, will attempt to perform the scene color downsample. Returns the filtered scene color, the downsampled
	// *  scene color (or null if it was not performed), and the secondary view rect.
	// */

	virtual FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const FPassInputs& PassInputs) const = 0;


	virtual float GetMinUpsampleResolutionFraction() const = 0;
	virtual float GetMaxUpsampleResolutionFraction() const = 0;

	virtual ITemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& ViewFamily) const = 0;

	static const ITemporalUpscaler* GetDefaultTemporalUpscaler();

	static EMainTAAPassConfig GetMainTAAPassConfig(const FViewInfo& View);
}; 
