// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

/**
 * Implements a globally unique identifier for network related use.
 */
class FNetworkGUID
{
public:

	union
	{
		UE_DEPRECATED(5.3, "Deprecated in favor of ObjectId")
		uint32 Value;
		uint64 ObjectId;
	};

	FNetworkGUID()
		: ObjectId(0)
	{ }

	UE_DEPRECATED(5.3, "No longer used")
	FNetworkGUID(uint32 V)
		: ObjectId(V)
	{ }
		
	friend bool operator==(const FNetworkGUID& X, const FNetworkGUID& Y)
	{
		return (X.ObjectId == Y.ObjectId);
	}

	friend bool operator!=(const FNetworkGUID& X, const FNetworkGUID& Y)
	{
		return (X.ObjectId != Y.ObjectId);
	}
	
	friend FArchive& operator<<(FArchive& Ar, FNetworkGUID& G)
	{
		Ar.SerializeIntPacked64(G.ObjectId);
		return Ar;
	}

	UE_DEPRECATED(5.3, "No longer used")
	void BuildFromNetIndex(int32 StaticNetIndex)
	{
		ObjectId = ((uint64)StaticNetIndex << 1 | 1);
	}

	UE_DEPRECATED(5.3, "No longer used")
	int32 ExtractNetIndex()
	{
		return ((int32)ObjectId & 1) ? (int32)ObjectId >> 1 : 0;
	}

	friend uint32 GetTypeHash(const FNetworkGUID& Guid)
	{
		return ::GetTypeHash(Guid.ObjectId);
	}

	bool IsDynamic() const
	{
		return IsValid() && !IsStatic();
	}

	bool IsStatic() const
	{
		return ObjectId & 1;
	}

	bool IsValid() const
	{
		return ObjectId > 0;
	}

	/** A Valid but unassigned NetGUID */
	bool IsDefault() const
	{
		return (ObjectId == 1);
	}

	static FNetworkGUID GetDefault()
	{
		return CreateFromIndex(0, true);
	}

	void Reset()
	{
		ObjectId = 0;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%" UINT64_FMT), ObjectId);
	}

	bool operator<(const FNetworkGUID& Other) const
	{
		return ObjectId < Other.ObjectId;
	}

	UE_DEPRECATED(5.3, "Deprecated in favor of CreateFromIndex")
	static FNetworkGUID Make(int32 Seed, bool bIsStatic)
	{
		return CreateFromIndex((uint64)Seed, bIsStatic);
	}

	static FNetworkGUID CreateFromIndex(uint64 NetIndex, bool bIsStatic)
	{
		check(NetIndex <= (MAX_uint64 >> 1));

		FNetworkGUID NewGuid;
		NewGuid.ObjectId = NetIndex << 1 | (bIsStatic ? 1 : 0);

		return NewGuid;
	}
};
