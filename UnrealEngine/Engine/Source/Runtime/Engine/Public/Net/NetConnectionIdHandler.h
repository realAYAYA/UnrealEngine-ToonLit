// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/BitArray.h"

class FNetConnectionIdHandler
{
public:
	ENGINE_API FNetConnectionIdHandler();
	ENGINE_API ~FNetConnectionIdHandler();

	ENGINE_API void Init(uint32 IdCount);

	ENGINE_API uint32 Allocate();
	ENGINE_API void Free(uint32 Id);

private:
	TBitArray<> UsedIds;
	uint32 IdHint;
};
