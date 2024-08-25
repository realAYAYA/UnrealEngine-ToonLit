// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Special flags for nDisplay preview log
 */

enum class EDisplayClusterViewportPreviewShowLogMsgOnce : uint8
{
	None = 0,

	// When CalculateStereoViewOffset() function has an error
	CalculateViewIsFailed = 1 << 0,

	StereoProjectionMatrixIsInvalid = 1 << 1,

};
ENUM_CLASS_FLAGS(EDisplayClusterViewportPreviewShowLogMsgOnce);
