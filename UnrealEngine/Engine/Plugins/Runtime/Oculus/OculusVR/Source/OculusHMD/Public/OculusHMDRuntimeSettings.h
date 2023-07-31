// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OculusHMDTypes.h"
#include "OculusFunctionLibrary.h"
#include "OculusHMDRuntimeSettings.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/**
* Implements the settings for the OculusVR plugin.
*/
UCLASS(config = Engine, defaultconfig, deprecated, meta = (DeprecationMessage = "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace."))
class OCULUSHMD_API UDEPRECATED_UOculusHMDRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	
	/** Whether the Splash screen is enabled. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = "Engine SplashScreen", meta = (DeprecatedProperty))
	bool bAutoEnabled;

	/** An array of splash screen descriptors listing textures to show and their positions. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = "Engine SplashScreen", meta = (DeprecatedProperty))
	TArray<FOculusSplashDesc> SplashDescs;

	/** This selects the XR API that the engine will use. If unsure, OVRPlugin OpenXR is the recommended API. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = General, meta = (DisplayName = "XR API", ConfigRestartRequired = true, DeprecatedProperty))
	EOculusXrApi XrApi;

	/** The target color space */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = General, meta = (DeprecatedProperty))
	EOculusColorSpace	ColorSpace;

	/** Whether Dash is supported by the app, which will keep the app in foreground when the User presses the oculus button (needs the app to handle input focus loss!) */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = PC, meta = (DeprecatedProperty))
	bool bSupportsDash;

	/** Whether the app's depth buffer is shared with the Rift Compositor, for layer (including Dash) compositing, PTW, and potentially more. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = PC, meta = (DeprecatedProperty))
	bool bCompositesDepth;

	/** Computes mipmaps for the eye buffers every frame, for a higher quality distortion */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = PC, meta = (DeprecatedProperty))
	bool bHQDistortion;

	/** Minimum allowed pixel density. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = PC, meta = (DeprecatedProperty))
	float PixelDensityMin;

	/** Maximum allowed pixel density. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = PC, meta = (DeprecatedProperty))
	float PixelDensityMax;

	/** A png for Mobile-OS-driven launch splash screen. It will show up instantly at app launch and disappear upon first engine-driven frame (regardless of said frame being UE4 splashes or 3D scenes) */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DisplayName = "OS Splash Screen", FilePathFilter = "png", RelativeToGameDir, DeprecatedProperty))
	FFilePath OSSplashScreen;

	/** Default CPU level controlling CPU frequency on the mobile device */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DeprecatedProperty))
	int CPULevel;

	/** Default GPU level controlling GPU frequency on the mobile device */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DeprecatedProperty))
	int GPULevel;

	/** If enabled the app will be focus aware. This will keep the app in foreground when the User presses the oculus button (needs the app to handle input focus loss!) */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DeprecatedProperty))
	bool bFocusAware;

	/** [Experimental]Enable Late latching for reducing HMD and controller latency, improve tracking prediction quality, multiview and vulkan must be enabled for this feature. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DeprecatedProperty))
	bool bLateLatching;

	/** If enabled the app will use the Oculus system keyboard for input fields. This requires that the app be focus aware. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DeprecatedProperty))
	bool bRequiresSystemKeyboard;

	/** Whether controllers and/or hands can be used with the app */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DeprecatedProperty))
	EHandTrackingSupport HandTrackingSupport;

	/** Note that a higher tracking frequency will reserve some performance headroom from the application's budget. */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DeprecatedProperty))
	EHandTrackingFrequency HandTrackingFrequency;

	/** Enable phase sync on mobile, reducing HMD and controller latency, improve tracking prediction quality */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (DeprecatedProperty))
	bool bPhaseSync;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif // WITH_EDITOR

private:
	void LoadFromIni();

};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
