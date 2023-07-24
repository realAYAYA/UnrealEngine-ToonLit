// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensTargetSettings.h"
#include "Misc/ConfigCacheIni.h"


/* UHoloLensTargetSettings structors
 *****************************************************************************/

UHoloLensTargetSettings::UHoloLensTargetSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MaxTrianglesPerCubicMeter(500.f)
	, SpatialMeshingVolumeSize(20.f)
{
}

void UHoloLensTargetSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Copy across from old location.
	FString DefaultConfigFile = GetDefaultConfigFilename();
	if (GConfig->DoesSectionExist(TEXT("/Script/HoloLensTargetPlatform.HoloLensTargetSettings"), DefaultConfigFile))
	{
		if (!GConfig->DoesSectionExist(TEXT("/Script/HoloLensPlatformEditor.HoloLensTargetSettings"), DefaultConfigFile))
		{
			FString OldTileBackgroundColorHex;
			FString OldSplashScreenBackgroundColorHex;
			GConfig->GetString(TEXT("/Script/HoloLensTargetPlatform.HoloLensTargetSettings"), TEXT("TileBackgroundColor"), OldTileBackgroundColorHex, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/HoloLensTargetPlatform.HoloLensTargetSettings"), TEXT("SplashScreenBackgroundColor"), OldSplashScreenBackgroundColorHex, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/HoloLensTargetPlatform.HoloLensTargetSettings"), TEXT("MinimumPlatformVersion"), MinimumPlatformVersion, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/HoloLensTargetPlatform.HoloLensTargetSettings"), TEXT("MaximumPlatformVersionTested"), MaximumPlatformVersionTested, DefaultConfigFile);

			TileBackgroundColor = FColor::FromHex(OldTileBackgroundColorHex);
			SplashScreenBackgroundColor = FColor::FromHex(OldSplashScreenBackgroundColorHex);

			TryUpdateDefaultConfigFile();
		}
		GConfig->EmptySection(TEXT("/Script/HoloLensTargetPlatform.HoloLensTargetSettings"), DefaultConfigFile);
		if (!GConfig->DoesSectionExist(TEXT("/Script/HoloLensPlatformEditor.HoloLensTargetSettings"), DefaultConfigFile))
		{
			TryUpdateDefaultConfigFile();
		}
	}

	// Determine if we need to set default capabilities for this project.
	GConfig->GetBool(TEXT("/Script/HoloLensPlatformEditor.HoloLensTargetSettings"), TEXT("bSetDefaultCapabilities"), bSetDefaultCapabilities, DefaultConfigFile);

	// Adjust the spatial mapping settings if needed
	if (MaxTrianglesPerCubicMeter < 1.f)
	{
		MaxTrianglesPerCubicMeter = 1.f;
	}
	if (SpatialMeshingVolumeSize < 1.f)
	{
		SpatialMeshingVolumeSize = 1.f;
	}
}
