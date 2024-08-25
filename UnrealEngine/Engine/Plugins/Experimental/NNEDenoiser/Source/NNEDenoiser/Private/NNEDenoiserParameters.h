// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

namespace UE::NNEDenoiser::Private
{
	
	struct FParameters
	{
		int32 TileMinimumOverlap = 20;
	};

} // namespace UE::NNEDenoiser::Private