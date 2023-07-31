// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

struct HOLOLENSTARGETPLATFORM_API FHoloLensSDKVersion
{
	/** The whole version string for the SDK */
	FString VersionString;
	/** The parsed parts of the version string (1.2.3.4) */
	int32 Version1;
	int32 Version2;
	int32 Version3;
	int32 Version4;

	FHoloLensSDKVersion()
	{
		Version1 = Version2 = Version3 = Version4 = 0;
	}

	FHoloLensSDKVersion(const FString& InVersionString, int32 InVer1, int32 InVer2, int32 InVer3, int32 InVer4)
		: VersionString(InVersionString)
		, Version1(InVer1)
		, Version2(InVer2)
		, Version3(InVer3)
		, Version4(InVer4)
	{
	}

	/** Returns the list of SDK versions for this machine */
	static TArray<FHoloLensSDKVersion> GetSDKVersions();
};