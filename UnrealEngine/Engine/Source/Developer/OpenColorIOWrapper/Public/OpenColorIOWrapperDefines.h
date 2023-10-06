// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

/** Enum used to indicate whether the working color space should be used as a source or destination. */
enum class EOpenColorIOWorkingColorSpaceTransform : uint8
{
	None = 0,
	Source = 1,
	Destination = 2
};

