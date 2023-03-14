// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDOF.h: Post process Depth of Field implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/PostProcessing.h"

class FViewInfo;
class FSceneTextureParameters;
struct FTranslucencyPassResources;
struct FTemporalAAHistory;


namespace DiaphragmDOF
{

// Whether DOF is enabled for the requested view.
bool IsEnabled(const FViewInfo& View);

float ComputeFocalLengthFromFov(const FSceneView& View);
FVector4f CircleDofHalfCoc(const FViewInfo& View);

/** Physically based circle of confusion computation model. */
struct FPhysicalCocModel
{
	// Unclamped resolution less background coc radius.
	float InfinityBackgroundCocRadius;

	// Resolution less minimal foreground coc radius < 0.
	float MinForegroundCocRadius;

	// Resolution less maximal background coc radius.
	float MaxBackgroundCocRadius;

	// Focus distance.
	float FocusDistance;

	// SqueezeFactor = VerticalFocalDistance / HorizontalFocalDistance
	float Squeeze;

	// The maximum radius of depth blur.
	float MaxDepthBlurRadius;
	float DepthBlurExponent;

	/** Compile the coc model from a view. */
	void Compile(const FViewInfo& View);
	
	/** Returns the CocRadius in half res pixels for given scene depth (in world unit).
	 *
	 * Notes: Matches Engine/Shaders/Private/DiaphragmDOF/Common.ush's SceneDepthToCocRadius().
	 */
	float DepthToResCocRadius(float SceneDepth, float HorizontalResolution) const;

	/** Returns limit(SceneDepthToCocRadius) for SceneDepth -> Infinity. */
	FORCEINLINE float ComputeViewMaxBackgroundCocRadius(float HorizontalResolution) const
	{
		return FMath::Min(FMath::Max(InfinityBackgroundCocRadius, MaxDepthBlurRadius), MaxBackgroundCocRadius) * HorizontalResolution;
	}
	
	/** Returns limit(SceneDepthToCocRadius) for SceneDepth -> 0.
	 *
	 * Note: this return negative or null value since this is foreground.
	 */
	FORCEINLINE float ComputeViewMinForegroundCocRadius(float HorizontalResolution) const
	{
		return DepthToResCocRadius(GNearClippingPlane, HorizontalResolution);
	}
};


enum class EBokehShape
{
	// No blade simulation.
	Circle,

	// Diaphragm's blades are straight.
	StraightBlades,

	// Diaphragm's blades are circle with a radius matching largest aperture of the lens system settings.
	RoundedBlades,
};


/** Model of bokeh to simulate a lens' diaphragm. */
struct FBokehModel
{
	// Shape of the bokeh.
	EBokehShape BokehShape;

	// Scale factor to transform a CocRadius to CircumscribedRadius or in circle radius.
	float CocRadiusToCircumscribedRadius;
	float CocRadiusToIncircleRadius;

	// Number of blades of the diaphragm.
	int32 DiaphragmBladeCount;
	
	// Rotation angle of the diaphragm in radians.
	float DiaphragmRotation;

	// BokehShape == RoundedBlades specific parameters.
	struct
	{
		// Radius of the blade for a boked area=PI.
		float DiaphragmBladeRadius;

		// Offset of the center of the blade's circle from the center of the bokeh.
		float DiaphragmBladeCenterOffset;
	} RoundedBlades;


	/** Compile the model from a view. */
	void Compile(const FViewInfo& View);
};


/** Returns whether DOF is supported. */
inline bool IsSupported(const FStaticShaderPlatform ShaderPlatform)
{
	// Only compile diaphragm DOF on platform it has been tested to ensure this is not blocking anyone else.
	return FDataDrivenShaderPlatformInfo::GetSupportsDiaphragmDOF(ShaderPlatform);
}


/** Wire all DOF's passes according to view settings and cvars to convolve the scene color. */
RENDERER_API FRDGTextureRef AddPasses(
	FRDGBuilder& GraphBuilder,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	FRDGTextureRef InputSceneColor,
	const FTranslucencyPassResources& TranslucencyViewResources);

} // namespace DiaphragmDOF
