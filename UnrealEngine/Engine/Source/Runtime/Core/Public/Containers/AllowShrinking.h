// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

// Specifies whether or not a container's removal operation should attempt to auto-shrink the container's reserved memory usage
enum class EAllowShrinking : uint8
{
	No,
	Yes
};

// Can comment in this when fixing up existing calls, or made permanent when it's ready to be
#define UE_ALLOWSHRINKING_BOOL_DEPRECATED(FunctionName) //UE_DEPRECATED(5.4, FunctionName " with a boolean bAllowShrinking has been deprecated - please use the EAllowShrinking enum instead")
