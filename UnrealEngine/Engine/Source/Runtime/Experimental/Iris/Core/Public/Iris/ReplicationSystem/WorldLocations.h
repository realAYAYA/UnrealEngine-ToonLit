// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ChunkedArray.h"
#include "Net/Core/NetBitArray.h"
#include "Math/Vector.h"

namespace UE::Net
{

struct FWorldLocationsInitParams
{
	uint32 MaxObjectCount = 0;
};

class FWorldLocations
{
public:
	void Init(const FWorldLocationsInitParams& InitParams);

	/** Returns whether the object has a valid world location or not. */
	bool HasWorldLocation(uint32 ObjectIndex) const;

	/** Returns the object's world location if it's valid or a zero vector if it's not. */
	FVector GetWorldLocation(uint32 ObjectIndex) const;

public:
	// Internal API
	void SetHasWorldLocation(uint32 ObjectIndex, bool bHasWorldLocation);
	void SetWorldLocation(uint32 ObjectIndex, const FVector& WorldLocation);

private:
	enum : uint32
	{
		BytesPerLocationChunk = 65536U,
	};

	FNetBitArray ValidWorldLocations;
	TChunkedArray<FVector, BytesPerLocationChunk> WorldLocations;
};

inline bool FWorldLocations::HasWorldLocation(uint32 ObjectIndex) const
{
	return ValidWorldLocations.GetBit(ObjectIndex);
}

inline FVector FWorldLocations::GetWorldLocation(uint32 ObjectIndex) const
{
	if (ValidWorldLocations.GetBit(ObjectIndex))
	{
		return WorldLocations[ObjectIndex];
	}
	else
	{
		return FVector::Zero();
	}
}

}

