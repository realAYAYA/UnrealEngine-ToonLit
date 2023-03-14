// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusPlatformToolSettings.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UDEPRECATED_UOculusPlatformToolSettings::UDEPRECATED_UOculusPlatformToolSettings()
	: OculusTargetPlatform(EOculusPlatformTarget::Rift)
{
	uint8 NumPlatforms = (uint8)EOculusPlatformTarget::Length;
	OculusApplicationID.Init("", NumPlatforms);
	OculusApplicationToken.Init("", NumPlatforms);
	OculusReleaseChannel.Init("Alpha", NumPlatforms);
	OculusReleaseNote.Init("", NumPlatforms);
	OculusLaunchFilePath.Init("", NumPlatforms);
	OculusSymbolDirPath.Init("", NumPlatforms);
	OculusLanguagePacksPath.Init("", NumPlatforms);
	OculusExpansionFilesPath.Init("", NumPlatforms);
	OculusAssetConfigs.Init(FAssetConfigArray(), NumPlatforms);
	UploadDebugSymbols = true;

	for (int i = 0; i < NumPlatforms; i++)
	{
		OculusAssetConfigs[i].ConfigArray = TArray<FAssetConfig>();
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
