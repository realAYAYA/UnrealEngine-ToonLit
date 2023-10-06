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

	/** Returns whether the object has a valid cached data or not. */
	bool HasInfoForObject(uint32 ObjectIndex) const;

	/** Returns the object's world location if it's valid or a zero vector if it's not. */
	FVector GetWorldLocation(uint32 ObjectIndex) const;

	/** Get the object's last cull distance we stored. */
	float GetCullDistance(uint32 ObjectIndex) const;

	/** Contains the cached object data we are storing. */
	struct FObjectInfo
	{
		/** Absolute coordinate of the object */
		FVector WorldLocation;
		/** Network cull distance of the object */
		float CullDistance;

		FObjectInfo() : WorldLocation(EForceInit::ForceInitToZero), CullDistance(0.0f) {}
	};

public:

	// Internal API
	void UpdateWorldLocation(uint32 ObjectIndex, const FVector& WorldLocation);

	void InitObjectInfoCache(uint32 ObjectIndex);
	void RemoveObjectInfoCache(uint32 ObjectIndex);

	void SetObjectInfo(uint32 ObjectIndex, const FObjectInfo& ObjectInfo);
    const FObjectInfo& GetObjectInfo(uint32 ObjectIndex) const;

private:
	enum : uint32
	{
		BytesPerLocationChunk = 65536U,
	};

	/** Set bits indicate that we have stored information for this internal object index */
	FNetBitArray ValidInfoIndexes;

	TChunkedArray<FObjectInfo, BytesPerLocationChunk> StoredObjectInfo;
};

inline bool FWorldLocations::HasInfoForObject(uint32 ObjectIndex) const
{
	return ValidInfoIndexes.IsBitSet(ObjectIndex);
}

inline const FWorldLocations::FObjectInfo& FWorldLocations::GetObjectInfo(uint32 ObjectIndex) const
{
	if (ValidInfoIndexes.IsBitSet(ObjectIndex))
	{
		return StoredObjectInfo[ObjectIndex];
	}

	static FObjectInfo EmptyInfo;
	return EmptyInfo;
}

inline FVector FWorldLocations::GetWorldLocation(uint32 ObjectIndex) const
{
	if (ValidInfoIndexes.IsBitSet(ObjectIndex))
	{
		return StoredObjectInfo[ObjectIndex].WorldLocation;
	}
	else
	{
		return FVector::Zero();
	}
}

inline float FWorldLocations::GetCullDistance(uint32 ObjectIndex) const
{
	if (ValidInfoIndexes.IsBitSet(ObjectIndex))
	{
		return StoredObjectInfo[ObjectIndex].CullDistance;
	}
	else
	{
		return 0.0f;
	}
}

}

