// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_Settings.h"
#include "ShowFlags.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FGameFrame
//-------------------------------------------------------------------------------------------------

class FGameFrame : public TSharedFromThis<FGameFrame, ESPMode::ThreadSafe>
{
public:
	uint32 FrameNumber;				// current frame number. (StartGameFrame_GameThread)
	float WorldToMetersScale;		// World units (UU) to Meters scale. (OnStartGameFrame)
	FVector2D WindowSize;			// actual window size (StartGameFrame_GameThread)
	FEngineShowFlags ShowFlags;		// (PreRenderViewFamily_RenderThread)

	FQuat PlayerOrientation;		// (CalculateStereoViewOffset)
	FVector PlayerLocation;			// (CalculateStereoViewOffset)
	float NearClippingPlane;		// (GetStereoProjectionMatrix)

	FTransform TrackingToWorld;		// (OnEndGameFrame)

	EFixedFoveatedRenderingLevel FFRLevel; // OnStartGameFrame
	bool FFRDynamic;					   //OnStartGameFrame

	ovrpFovf Fov[ovrpEye_Count];	// UpdateStereoRenderingParams

	union
	{
		struct
		{
			/** True, if splash is shown */
			uint64			bSplashIsShown : 1;
			/** True, if spectator screen is active */
			uint64			bSpectatorScreenActive : 1;
			/** True if the frame's positions have been updated on the render thread */
			uint64			bRTLateUpdateDone : 1;
		};
		uint64 Raw;
	} Flags;

public:
	FGameFrame();

	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> Clone() const;
};

typedef TSharedPtr<FGameFrame, ESPMode::ThreadSafe> FGameFramePtr;

} // namespace OculusHMD

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
