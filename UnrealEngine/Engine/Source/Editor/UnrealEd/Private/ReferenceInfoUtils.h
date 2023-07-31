// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ReferenceInfoUtils
{
	/**
	 * Outputs reference info to a log file
	 */
	void GenerateOutput(UWorld* InWorld, int32 Depth, bool bShowDefault, bool bShowScript);
}
