// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Containers/ChunkedArray.h"
#include "Net/Core/NetBitArray.h"

namespace UE::Net
{

struct FNetCullDistanceOverridesInitParams
{
	uint32 MaxObjectCount = 0;
};

class FNetCullDistanceOverrides
{
public:
	void Init(const FNetCullDistanceOverridesInitParams& InitParams);

	/** Returns whether the object has a valid world location or not. */
	bool HasCullDistanceOverride(uint32 ObjectIndex) const;

	/** Returns the object's squared cull distance override if it's valid or a negative value if it's not. */
	float GetCullDistanceSqr(uint32 ObjectIndex) const;

	/** Returns the object's squared cull distance override if it's valid or the provided DefaultValue if it's not. */
	float GetCullDistanceSqr(uint32 ObjectIndex, float DefaultValue) const;

public:
	/** Remove cull distance override for object. */
	void ClearCullDistanceSqr(uint32 ObjectIndex);

	/** Set cull distance override for object. */
	void SetCullDistanceSqr(uint32 ObjectIndex, float CullDistSqr);

private:
	enum : unsigned
	{
		// Arbitrary value. The chunked array will be allocated to fit to higest object index that overrides the cull distance.
		BytesPerCullDistanceChunk = 4096U,
	};

	FNetBitArray ValidCullDistanceSqr;
	TChunkedArray<float, BytesPerCullDistanceChunk> CullDistanceSqr;
};

inline bool FNetCullDistanceOverrides::HasCullDistanceOverride(uint32 ObjectIndex) const
{
	return ValidCullDistanceSqr.GetBit(ObjectIndex);
}

inline float FNetCullDistanceOverrides::GetCullDistanceSqr(uint32 ObjectIndex, float DefaultValue) const
{
	if (ValidCullDistanceSqr.GetBit(ObjectIndex))
	{
		return CullDistanceSqr[ObjectIndex];
	}
	else
	{
		return DefaultValue;
	}
}

inline float FNetCullDistanceOverrides::GetCullDistanceSqr(uint32 ObjectIndex) const
{
	return GetCullDistanceSqr(ObjectIndex, -1.0f);
}

}

