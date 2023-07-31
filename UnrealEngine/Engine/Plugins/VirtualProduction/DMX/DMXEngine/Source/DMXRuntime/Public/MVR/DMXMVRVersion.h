// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/** Version of the MVR standard currently being used by the engine */
struct DMXRUNTIME_API FDMXMVRVersion
{
	static const int32 MajorVersion = 1;

	static const int32 MinorVersion = 4;

	static const FString GetMajorVersionAsString();
	static const FString GetMinorVersionAsString();

	/** Returns true if the provided version is the latest version */
	static bool IsLatest(int32 InMajorVersion, int32 InMinorVersion)
	{
		return
			InMajorVersion == MajorVersion &&
			InMinorVersion == MinorVersion;
	}
};
