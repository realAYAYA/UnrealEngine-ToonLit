// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

union FPhysicalTileLocation
{
	FPhysicalTileLocation() {}
	FPhysicalTileLocation(const FIntVector& InVec)
		: TileX(InVec.X)
		, TileY(InVec.Y)
	{
		checkSlow(InVec.X >= 0 && InVec.X <= 255);
		checkSlow(InVec.Y >= 0 && InVec.Y <= 255);
	}

	uint16 Packed;
	struct 
	{
		uint8 TileX;
		uint8 TileY;
	};
};

struct FPageTableUpdate
{
	uint32					vAddress;
	FPhysicalTileLocation	pTileLocation;
	uint8					vLevel;
	uint8					vLogSize;

	FPageTableUpdate() {}
	FPageTableUpdate(const FPageTableUpdate& Other) = default;
	FPageTableUpdate& operator=(const FPageTableUpdate& Other) = default;

	FPageTableUpdate(const FPageTableUpdate& Update, uint32 Offset, uint8 vDimensions)
		: vAddress(Update.vAddress + (Offset << (vDimensions * Update.vLogSize)))
		, pTileLocation(Update.pTileLocation)
		, vLevel(Update.vLevel)
		, vLogSize(Update.vLogSize)
	{}

	inline void Check(uint8 vDimensions)
	{
		const uint32 LowBitMask = (1u << (vDimensions * vLogSize)) - 1;
		check((vAddress & LowBitMask) == 0);
		//checkSlow(vLogSize <= vLevel);
	}
};
