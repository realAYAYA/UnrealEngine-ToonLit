// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HoloLensTargetSettings.h: Declares the UHoloLensTargetSettings class.
=============================================================================*/

#pragma once

#include "WindowsTargetSettings.h"
#include "HoloLensLocalizedResources.h"
#include "HoloLensTargetSettings.generated.h"

/**
 * Implements the settings for the HoloLens target platform.
 */
UCLASS(config = Engine, defaultconfig)
class UHoloLensTargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()

	virtual const TCHAR* GetConfigOverridePlatform() const override
	{
		return TEXT("HoloLens");
	}

	virtual void PostInitProperties() override;

	/**
	 * When checked, a build that can be run via emulation is added
	 */
	UPROPERTY(EditAnywhere, Config, Category="HoloLens", Meta=(DisplayName="Build for HoloLens Emulation"))
	bool bBuildForEmulation = false;

	/**
	 * When the box checked the final bundle has binaries of ARM64 OSes.
	 */
	UPROPERTY(EditAnywhere, Config, Category="HoloLens", Meta=(DisplayName="Build for HoloLens Device"))
	bool bBuildForDevice = true;

	UPROPERTY(EditAnywhere, config, Category = "Packaging", AdvancedDisplay, Meta = (DisplayName = "Use Name in App Logo"))
	bool bUseNameForLogo = true;

	/**
	 * Controls whether to use the retail Windows Store environment for license checks.  This must be turned on
	 * when building for submission to the Windows Store, or when sideloading outside of Developer Mode.  Note,
	 * however, that testing a build with this flag enables requires that the product is listed in the retail
	 * catalog for the Windows Store.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Packaging", AdvancedDisplay, Meta = (DisplayName = "Use Retail Windows Store Environment"))
	bool bBuildForRetailWindowsStore;
	
	UPROPERTY(EditAnywhere, Config, Category = "Packaging")
	bool bAutoIncrementVersion;
	
	UPROPERTY(EditAnywhere, Config, Category = "App Installer")
	bool bShouldCreateAppInstaller;

	UPROPERTY(EditAnywhere, Config, Category = "App Installer")
	FString AppInstallerInstallationURL;

	UPROPERTY(EditAnywhere, Config, Category = "App Installer", meta = (ToolTip = "0 will check on every app launch."))
	int HoursBetweenUpdateChecks = 0;

	UPROPERTY(EditAnywhere, config, Category = "Rendering", meta = (DisplayName = "Enable PIX Profiling"))
	bool bEnablePIXProfiling;

	UPROPERTY(EditAnywhere, config, Category = Packaging, meta=(HideAlphaChannel))
	FColor TileBackgroundColor = FColor::FromHex(FString("#000040"));

	UPROPERTY(EditAnywhere, config, Category = Packaging, meta=(HideAlphaChannel))
	FColor SplashScreenBackgroundColor = FColor::FromHex(FString("#000040"));

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	TArray<FHoloLensCorePackageLocalizedResources> PerCultureResources;

	/**
	 * Identifies the device family that your package will target.
	 */
	UPROPERTY(EditAnywhere, config, Category = "OS Info")
	FString TargetDeviceFamily;

	/**
	 * Minimum version of the HoloLens platform required to run this title.
	 * It will not be possible to deploy the build on earlier versions.
	 */
	UPROPERTY(EditAnywhere, config, Category = "OS Info", AdvancedDisplay, Meta = (DisplayName = "Minimum supported platform version"))
	FString MinimumPlatformVersion;

	/**
	 * Specifies the maximum version of the universal platform on which the
	 * title is known to work as expected.  When deployed to later versions
	 * the title will experience the runtime behavior of the version given here.
	 */
	UPROPERTY(EditAnywhere, config, Category = "OS Info", AdvancedDisplay, Meta = (DisplayName = "Maximum tested platform version"))
	FString MaximumPlatformVersionTested;

	/** Used by the HoloLens to indicate triangle density when generating meshes. Defaults to 500 per cubic meter */
	UPROPERTY(EditAnywhere, Config, Category="Spatial Mapping")
	float MaxTrianglesPerCubicMeter;

	/** Used by the HoloLens to indicate how large (in meters) of a volume to use for generating meshes. Defaults to a 20m cube */
	UPROPERTY(EditAnywhere, Config, Category = "Spatial Mapping")
	float SpatialMeshingVolumeSize;

	/** The compiler version to use for this project. May be different to the chosen IDE. */
	UPROPERTY(EditAnywhere, config, Category = "Toolchain", Meta = (DisplayName = "Compiler Version"))
	ECompilerVersion CompilerVersion;

	UPROPERTY(EditAnywhere, config, Category = "Toolchain", Meta = (DisplayName = "Windows 10 SDK Version"))
	FString Windows10SDKVersion;

	/**
	 * List of supported <Capability><Capability> elements for the application.
	 */
	UPROPERTY(EditAnywhere, config, Category = Capabilities)
	TArray<FString> CapabilityList;

	/**
	 * List of supported <Capability><DeviceCapability> elements for the application.
	 */
	UPROPERTY(EditAnywhere, config, Category = Capabilities)
	TArray<FString> DeviceCapabilityList;

	/**
	 * List of supported <Capability><uap:Capability> elements for the application.
	 */
	UPROPERTY(EditAnywhere, config, Category = Capabilities)
	TArray<FString> UapCapabilityList;

	/**
	 * List of supported <Capability><uap2:Capability> elements for the application.
	 */
	UPROPERTY(EditAnywhere, config, Category = Capabilities)
	TArray<FString> Uap2CapabilityList;

	/**
	 * Set default capabilities (InternetClientServer and PrivateNetworkClientServer) for the application.
	 */
	UPROPERTY(EditAnywhere, config, Category = Capabilities)
	bool bSetDefaultCapabilities = true;

	/** Which of the currently enabled spatialization plugins to use. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SpatializationPlugin;

	/** Which of the currently enabled source data override plugins to use. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SourceDataOverridePlugin;

	/** Which of the currently enabled reverb plugins to use. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString ReverbPlugin;

	/** Which of the currently enabled occlusion plugins to use. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString OcclusionPlugin;

	/** Quality Level to COOK SoundCues at (if set, all other levels will be stripped by the cooker). */
	UPROPERTY(config, EditAnywhere, Category = "Audio|CookOverrides", meta = (DisplayName = "Sound Cue Cook Quality"))
	int32 SoundCueCookQualityIndex = INDEX_NONE;
};
