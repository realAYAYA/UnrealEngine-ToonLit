// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Process exit codes */
enum class EMovieSceneCaptureExitCode : uint32
{
	Base = 0xAC701000,
	AssetNotFound,
	WorldNotFound,
};
