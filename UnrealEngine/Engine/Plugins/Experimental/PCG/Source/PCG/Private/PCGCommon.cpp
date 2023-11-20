// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGCommon.h"

namespace PCGFeatureSwitches
{

	TAutoConsoleVariable<bool> CVarCheckSamplerMemory{
		TEXT("pcg.CheckSamplerMemory"),
		true,
		TEXT("Checks expected memory size consumption prior to performing sampling operations")
	};

}

namespace PCGHiGenGrid
{
	bool IsValidGridSize(uint32 InGridSize)
	{
		// Must be a power of 2 (in m) and within the valid range
		// TODO: support other units
		return FMath::IsPowerOfTwo(InGridSize / 100)
			&& InGridSize >= GridToGridSize(EPCGHiGenGrid::GridMin)
			&& InGridSize <= GridToGridSize(EPCGHiGenGrid::GridMax);
	}

	bool IsValidGrid(EPCGHiGenGrid InGrid)
	{
		// Check the bitmask value is within range
		return InGrid >= EPCGHiGenGrid::GridMin && static_cast<uint32>(InGrid) < 2 * static_cast<uint32>(EPCGHiGenGrid::GridMax);
	}

	uint32 GridToGridSize(EPCGHiGenGrid InGrid)
	{
		const uint32 GridAsUint = static_cast<uint32>(InGrid);
		ensure(FMath::IsPowerOfTwo(GridAsUint));
		// TODO: support other units
		return IsValidGrid(InGrid) ? (GridAsUint * 100) : UninitializedGridSize();
	}

	EPCGHiGenGrid GridSizeToGrid(uint32 InGridSize)
	{
		if (InGridSize == UnboundedGridSize())
		{
			return EPCGHiGenGrid::Unbounded;
		}
		// TODO: support other units
		return ensure(IsValidGridSize(InGridSize)) ? static_cast<EPCGHiGenGrid>(InGridSize / 100) : EPCGHiGenGrid::Uninitialized;
	}

	uint32 UninitializedGridSize()
	{
		return static_cast<uint32>(EPCGHiGenGrid::Uninitialized);
	}

	uint32 UnboundedGridSize()
	{
		// TODO: support other units
		return 100 * static_cast<uint32>(EPCGHiGenGrid::Unbounded);
	}
}

namespace PCGDelegates
{
#if WITH_EDITOR
	FOnInstanceLayoutChanged OnInstancedPropertyBagLayoutChanged;
#endif
} // PCGDelegates