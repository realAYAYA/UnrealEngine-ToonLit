// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShadingRateAttachment.h: Variable rate shading attachment generation class
	definitions.
=============================================================================*/

#pragma once

#include "RHI.h"
#include "RendererInterface.h"
#include "RenderGraphDefinitions.h"

struct FMinimalSceneTextures; // Forward declaration from SceneTextures.

class IVariableRateShadingImageGenerator;

class FVariableRateShadingImageManager : public FRenderResource
{
public:
	/**
	 * Pass type used by some generators. Must be provided by passes requesting a VRS image.
	 */
	enum EVRSPassType
	{
		BasePass,
		TranslucencyAll,
		NaniteEmitGBufferPass,
		SSAO,
		SSR,
		ReflectionEnvironmentAndSky,
		LightFunctions,
		Decals,
		Num
	};

	/**
	 * Explicit VRS source flags; each type of VRS image generated here will have it's own flag defined in this bitfield.
	 * These flags can be used for any particular rendering path to exclude specific types of VRS generation; e.g.
	 * Used to allow exclusion of certain types when getting image.
	 */
	enum class EVRSSourceType : uint32
	{
		/** Fixed Foveation: Shading rate is decreased near the periphery of the viewport */
		FixedFoveation = 0x1,

		/** Eye-Tracked Foveation: Shading rate is decreased in the periphery of the user's gaze */
		EyeTrackedFoveation = 0x2,

		/** Contrast Adaptive Shading: Shading rate is decreased in areas of low contrast (calculated based on the prior frame) */
		ContrastAdaptiveShading = 0x4,

		// @todo: Add more types here as they're implemented.

		/** Mask of all available VRS source types. */
		All = FixedFoveation | EyeTrackedFoveation | ContrastAdaptiveShading,

		/** Mask of XR foveated VRS gen only. */
		XRFoveation = FixedFoveation | EyeTrackedFoveation,

		None = 0x0,
	};

	FVariableRateShadingImageManager();
	virtual ~FVariableRateShadingImageManager();

	virtual void ReleaseDynamicRHI() override;

	/**
	 * Get the combined VRS image for the specified view setup, target size/configuration, and application flags.
	 * May be run multiple times from different passes.
	 */
	FRDGTextureRef GetVariableRateShadingImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, EVRSPassType PassType,
		const TArray<TRefCountPtr<IPooledRenderTarget>>* ExternalVRSSources, FVariableRateShadingImageManager::EVRSSourceType VRSTypesToExclude = FVariableRateShadingImageManager::EVRSSourceType::None);

	/**
	 * Prepare VRS images and store them for later access.
	 * Should be run exactly once in Render(), before attempting to get any VRS images for that frame.
	 */
	void PrepareImageBasedVRS(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures);

	/**
	 * Returns true if any generator among the given types is enabled, false otherwise.
	 */
	bool IsTypeEnabledForView(const FSceneView& View, EVRSSourceType Type) const;


	/** 
	 *  Special case (currently) for mobile devices; right now this will return either no image, or a static fixed-foveation shading rate image
	 *  provided by the stereo device runtime.
	 */
	TRefCountPtr<IPooledRenderTarget> GetMobileVariableRateShadingImage(const FSceneViewFamily& ViewFamily);

	static bool IsVRSSupportedByRHI();
	static bool IsVRSCompatibleWithView(const FViewInfo& View);
	static bool IsVRSCompatibleWithOutputType(const EDisplayOutputFormat& DisplayOutputFormat);

	static FIntPoint GetSRITileSize();

	void DrawDebugPreview(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, FRDGTextureRef OutputSceneColor);

private:
	TRefCountPtr<IPooledRenderTarget> MobileHMDFixedFoveationOverrideImage;
	TArray<TUniquePtr<IVariableRateShadingImageGenerator>> ImageGenerators;

	FRDGTextureRef CombineShadingRateImages(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, TArray<FRDGTextureRef> Sources);
};

ENUM_CLASS_FLAGS(FVariableRateShadingImageManager::EVRSSourceType);

RENDERER_API extern TGlobalResource<FVariableRateShadingImageManager> GVRSImageManager;

class IVariableRateShadingImageGenerator
{
public:
	virtual ~IVariableRateShadingImageGenerator() {};

	// Returns cached VRS image.
	virtual FRDGTextureRef GetImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSPassType PassType) = 0;

	// Generates image(s) and saves to generator cache. Should only be run once per view per frame, in Render().
	virtual void PrepareImages(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures) = 0;

	// Returns whether or not generator is enabled - can change at runtime
	virtual bool IsEnabledForView(const FSceneView& View) const { return false; };

	// Return bitmask of generator type
	virtual FVariableRateShadingImageManager::EVRSSourceType GetType() const { return FVariableRateShadingImageManager::EVRSSourceType::None; };

	// Get VRS image to be used w/ debug overlay
	virtual FRDGTextureRef GetDebugImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo) = 0;
};