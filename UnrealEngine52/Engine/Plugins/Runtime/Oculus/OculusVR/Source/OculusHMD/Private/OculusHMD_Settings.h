// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS

PRAGMA_DISABLE_DEPRECATION_WARNINGS

namespace OculusHMD
{

static const float ClampPixelDensityMin = 0.5f;
static const float ClampPixelDensityMax = 2.0f;


//-------------------------------------------------------------------------------------------------
// FSettings
//-------------------------------------------------------------------------------------------------

class FSettings : public TSharedFromThis<FSettings, ESPMode::ThreadSafe>
{
public:
	union
	{
		struct
		{
			/** Whether stereo is currently on or off. */
			uint64 bStereoEnabled : 1;

			/** Whether or not switching to stereo is allowed */
			uint64 bHMDEnabled : 1;

			/** Turns on/off updating view's orientation/position on a RenderThread. When it is on,
				latency should be significantly lower.
				See 'HMD UPDATEONRT ON|OFF' console command.
			*/
			uint64 bUpdateOnRT : 1;

			/** Enforces headtracking to work even in non-stereo mode (for debugging or screenshots).
				See 'MOTION ENFORCE' console command. */
			uint64 bHeadTrackingEnforced : 1;

			/** Allocate an high quality OVR_FORMAT_R11G11B10_FLOAT buffer for Rift */
			uint64 bHQBuffer : 1;

			/** Rendering should be (could be) paused */
			uint64				bPauseRendering : 1;

			/** HQ Distortion */
			uint64				bHQDistortion : 1;

			/** Send the depth buffer to the compositor */
			uint64				bCompositeDepth : 1;

			/** Supports Dash in-game compositing */
			uint64				bSupportsDash : 1;
#if !UE_BUILD_SHIPPING
			/** Show status / statistics on screen. See 'hmd stats' cmd */
			uint64				bShowStats : 1;
#endif
			/** Dynamically update pixel density to maintain framerate */
			uint64				bPixelDensityAdaptive : 1;

			/** All future eye buffers will need to be created with TexSRGB_Create flag due to the current feature level (ES31) */
			uint64				bsRGBEyeBuffer : 1;

			/** Supports Focus Aware state on Quest */
			uint64				bFocusAware : 1;

			/** Requires the Oculus system keyboard */
			uint64				bRequiresSystemKeyboard : 1;
		};
		uint64 Raw;
	} Flags;

	/** HMD base values, specify forward orientation and zero pos offset */
	FVector BaseOffset; // base position, in meters, relatively to the sensor //@todo hmd: clients need to stop using oculus space
	FQuat BaseOrientation; // base orientation

	/** Viewports for each eye, in render target texture coordinates */
	FIntRect EyeRenderViewport[2];
	/** Viewports for each eye, without DynamicResolution scaling applied */
	FIntRect EyeUnscaledRenderViewport[2];

	ovrpMatrix4f EyeProjectionMatrices[2]; // 0 - left, 1 - right same as Views
	ovrpMatrix4f MonoProjectionMatrix;

	FIntPoint RenderTargetSize;
	float PixelDensity;
	float PixelDensityMin;
	float PixelDensityMax;

	ovrpSystemHeadset SystemHeadset;

	float VsyncToNextVsync;

	EFixedFoveatedRenderingLevel FFRLevel;
	bool FFRDynamic;
	int CPULevel;
	int GPULevel;

	EOculusXrApi XrApi;
	EOculusColorSpace ColorSpace;

	EHandTrackingSupport HandTrackingSupport;
	EHandTrackingFrequency HandTrackingFrequency;

	ovrpVector4f ColorScale, ColorOffset;
	bool bApplyColorScaleAndOffsetToAllLayers;

        EShaderPlatform CurrentShaderPlatform;

	bool bLateLatching;
	bool bPhaseSync;

public:
	FSettings();
	virtual ~FSettings() {}

	bool IsStereoEnabled() const { return Flags.bStereoEnabled && Flags.bHMDEnabled; }

	void SetPixelDensity(float NewPixelDensity);
	void SetPixelDensityMin(float NewPixelDensityMin);
	void SetPixelDensityMax(float NewPixelDensityMax);
	
	TSharedPtr<FSettings, ESPMode::ThreadSafe> Clone() const;
};

typedef TSharedPtr<FSettings, ESPMode::ThreadSafe> FSettingsPtr;

} // namespace OculusHMD

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
