// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Iris/IrisConfig.h"
#include "Containers/StringFwd.h"
#include "Templates/TypeHash.h"

// Forward declarations
class FString;

namespace UE::Net::Private
{
	class FNetObjectGroups;
}

namespace UE::Net
{

class FNetObjectGroupHandle
{
public:
	typedef uint16 FGroupIndexType;

	/** Reserved group indices */
	enum : FGroupIndexType
	{
		InvalidNetObjectGroupIndex = 0,
		NotReplicatedNetObjectGroupIndex,
		NetGroupOwnerNetObjectGroupIndex,
		NetGroupReplayNetObjectGroupIndex
	};

	inline static FNetObjectGroupHandle GetInvalid() {return FNetObjectGroupHandle();}

	FNetObjectGroupHandle() : Value(0u) {}

	/** Returns true if the handle is valid, note this does not mean that the group is valid */
	inline bool IsValid() const { return Value != 0u; }

	/** Returns the GroupIndex of the group assosciated with the handle */
	inline FGroupIndexType GetGroupIndex() const { return FGroupIndexType(Index); }

	/** Returns true if the provided GroupIndex is a reserved NetObjectsGroupIndex */
	static bool IsReservedNetObjectGroupIndex(FGroupIndexType GroupIndex) { return GroupIndex >= NotReplicatedNetObjectGroupIndex && GroupIndex <= NetGroupReplayNetObjectGroupIndex; }

	/** Returns true if this is a reserved NetObjectsGroups */
	bool IsReservedNetObjectGroup() const { return FNetObjectGroupHandle::IsReservedNetObjectGroupIndex(Index); }

	/** Special group, NetHandles assigned to this group will be filtered out for all connections */
	bool IsNotReplicatedNetObjectGroup() const { return Index == NotReplicatedNetObjectGroupIndex; }

	/** Special group, SubObjects assigned to this group will replicate to owner of RootParent */
	bool IsNetGroupOwnerNetObjectGroup() const { return Index == NetGroupOwnerNetObjectGroupIndex; }

	/** Special group, SubObjects assigned to this group will replicate if replay netconditions is met  */
	bool IsNetGroupReplayNetObjectGroup() const { return Index == NetGroupReplayNetObjectGroupIndex; }

	uint32 GetRawValue() const
	{
		return Value;
	}
	
private:
	friend UE::Net::Private::FNetObjectGroups;

	FNetObjectGroupHandle(FGroupIndexType IndexIn, FGroupIndexType EpochIn)
	{ 
		if (IndexIn == InvalidNetObjectGroupIndex)
		{
			*this = FNetObjectGroupHandle();
		}
		else
		{
			Index = IndexIn;
			Epoch = EpochIn;
		}
	}

	union 
	{
		uint32 Value;
		struct
		{
			FGroupIndexType Index;
			FGroupIndexType Epoch;
		};
	};

	friend inline bool operator==(const FNetObjectGroupHandle& Lhs, const FNetObjectGroupHandle& Rhs) { return Lhs.Value == Rhs.Value; }
	friend inline bool operator!=(const FNetObjectGroupHandle& Lhs, const FNetObjectGroupHandle& Rhs) { return Lhs.Value != Rhs.Value; }
};

static_assert(sizeof(FNetObjectGroupHandle) == sizeof(uint32), "Bad packing");

FORCEINLINE uint32 GetTypeHash(const FNetObjectGroupHandle Handle)
{
	return ::GetTypeHash(Handle.GetGroupIndex());
}
}
