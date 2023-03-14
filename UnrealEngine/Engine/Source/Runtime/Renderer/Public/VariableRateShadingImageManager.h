// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShadingRateAttachment.h: Variable rate shading attachment generation class
	definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "RenderGraphDefinitions.h"

/**
 * Explicit VRS source flags; each type of VRS image generated here will have it's own flag defined in this bitfield.
 * These flags can be used for any particular rendering path to exclude specific types of VRS generation; e.g. 
 * 
 */
enum class EVRSType : uint32
{
	/** VRS Image to be applied during base pass. */
	FixedFoveation = 0x1,

	/** VRS Image to be applied during post-processing. */
	EyeTrackedFoveation = 0x2,

	// @todo: Add more types here as they're implemented.

	/** Mask of all available VRS source types. */
	All = FixedFoveation | EyeTrackedFoveation,

	/** Mask of XR fixed-foveation VRS gen only. */
	XRFoveation = FixedFoveation | EyeTrackedFoveation,

	None = 0x0,
};

ENUM_CLASS_FLAGS(EVRSType);

enum class EVRSGenerationFlags : uint32;

struct FVRSImageGenerationParameters;

struct FDynamicVRSData
{
	float	VRSAmount = 1.0f;
	double	SumBusyTime = 0.0;
	int		NumFramesStored = 0;
	uint32	LastUpdateFrame = 0;
};

class FVariableRateShadingImageManager : public FRenderResource
{
public:
	FVariableRateShadingImageManager();
	virtual ~FVariableRateShadingImageManager();

	virtual void ReleaseDynamicRHI() override;

	/** Get the VRS image for the specified view setup, target size/configuration, and application flags.
	 *  If the VRS image does not exist, or is out of date, it will be rendered.
	 *  Static VRS images will be kept until until no longer referenced. */
	FRDGTextureRef GetVariableRateShadingImage(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const TArray<TRefCountPtr<IPooledRenderTarget>>* ExternalVRSSources, EVRSType VRSTypesToExclude = EVRSType::None);

	/** Special case (currently) for mobile devices; right now this will return either no image, or a static fixed-foveation shading rate image
	 *  provided by the stereo device runtime. */
	TRefCountPtr<IPooledRenderTarget> GetMobileVariableRateShadingImage(const FSceneViewFamily& ViewFamily);

protected:
	TRefCountPtr<IPooledRenderTarget> RenderShadingRateImage(FRDGBuilder& GraphBuilder, uint64 Key, const FVRSImageGenerationParameters& VRSImageGenParamsIn, EVRSGenerationFlags GenFlags);
	uint64 CalculateVRSImageHash(const FVRSImageGenerationParameters& VRSImageGenParamsIn, EVRSGenerationFlags ViewFlags) const;
	void UpdateFixedFoveationParameters(FVRSImageGenerationParameters& VRSImageGenParamsInOut);
	void UpdateEyeTrackedFoveationParameters(FVRSImageGenerationParameters& VRSImageGenParamsInOut, const FSceneViewFamily& ViewFamily);
	float GetDynamicVRSAmount();

	/** Per frame tick/update. */
	void Tick();

private:
	struct FActiveTarget
	{
		explicit FActiveTarget(const TRefCountPtr<IPooledRenderTarget>& TargetIn)
			: Target(TargetIn)
			, LastUsedFrame(GFrameNumber)
		{}

		FActiveTarget(FActiveTarget&& Other)
			: Target(MoveTemp(Other.Target))
			, LastUsedFrame(Other.LastUsedFrame)
		{}

		TRefCountPtr<IPooledRenderTarget> Target;
		uint64 LastUsedFrame = 0;
	};

	TMap<uint64, FActiveTarget> ActiveVRSImages;
	TRefCountPtr<IPooledRenderTarget> MobileHMDFixedFoveationOverrideImage;

	uint64 LastFrameTick;
	FDynamicVRSData DynamicVRSData;
};

RENDERER_API extern TGlobalResource<FVariableRateShadingImageManager> GVRSImageManager;
