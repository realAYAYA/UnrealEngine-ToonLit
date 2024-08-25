// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Particle/ObjectState.h"

class FString;

namespace Chaos
{
	CHAOS_API FString ToString(const EObjectStateType ObjectState);
	CHAOS_API FString ToString(const TBitArray<>& BitArray);
}