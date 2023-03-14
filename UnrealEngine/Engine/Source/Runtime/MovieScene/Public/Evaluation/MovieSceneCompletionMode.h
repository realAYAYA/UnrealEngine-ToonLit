// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneCompletionMode.generated.h"

/** Enumeration specifying how to handle state when this section is no longer evaluated */
UENUM(BlueprintType)
enum class EMovieSceneCompletionMode : uint8
{
	KeepState,

	RestoreState,

	ProjectDefault,
};