// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/** Specifications of channels in a universe displayed in UI */
struct FDMXChannelGridSpecs
{
	/** Num Columns in the grid */
	static const int32 NumColumns;

	/** The offset of the Channel ID, from 0 to the first channel ID */
	static const int32 ChannelIDOffset;
};
