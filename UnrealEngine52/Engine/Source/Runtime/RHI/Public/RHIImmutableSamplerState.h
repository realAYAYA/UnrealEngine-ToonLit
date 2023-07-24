// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/StaticArray.h"

class FRHISamplerState;

/** Maximum number of immutable samplers in a PSO. */
enum
{
	MaxImmutableSamplers = 2
};

struct FImmutableSamplerState
{
	using TImmutableSamplers = TStaticArray<FRHISamplerState*, MaxImmutableSamplers>;

	FImmutableSamplerState()
		: ImmutableSamplers(InPlace, nullptr)
	{}

	void Reset()
	{
		for (uint32 Index = 0; Index < MaxImmutableSamplers; ++Index)
		{
			ImmutableSamplers[Index] = nullptr;
		}
	}

	bool operator==(const FImmutableSamplerState& rhs) const
	{
		return ImmutableSamplers == rhs.ImmutableSamplers;
	}

	bool operator!=(const FImmutableSamplerState& rhs) const
	{
		return ImmutableSamplers != rhs.ImmutableSamplers;
	}

	TImmutableSamplers ImmutableSamplers;
};
