// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/SparseArray.h"

namespace UE::Net
{
	typedef uint16 FNetObjectGroupHandle;
	namespace Private
	{
		typedef uint32 FInternalNetHandle;
	}
}

namespace UE::Net::Private
{

enum class ENetObjectGroupTraits : uint32
{
	None = 0U,
	IsFindableByName = 1U << 0U,
};
ENUM_CLASS_FLAGS(ENetObjectGroupTraits);

struct FNetObjectGroup
{
	FNetObjectGroup() : Traits(ENetObjectGroupTraits::None) {}

	// Group members can only be replicated objects that have internal indices
	TArray<uint32> Members;
	FName GroupName;
	ENetObjectGroupTraits Traits;
};

struct FNetObjectGroupInitParams
{
	uint32 MaxObjectCount;
	uint32 MaxGroupCount;
};

class FNetObjectGroups
{
	UE_NONCOPYABLE(FNetObjectGroups)

public:
	FNetObjectGroups();
	~FNetObjectGroups();

	void Init(const FNetObjectGroupInitParams& Params);

	FNetObjectGroupHandle CreateGroup();
	void DestroyGroup(FNetObjectGroupHandle GroupHandle);
	void ClearGroup(FNetObjectGroupHandle GroupHandle);
	
	const FNetObjectGroup* GetGroup(FNetObjectGroupHandle GroupHandle) const;
	FNetObjectGroup* GetGroup(FNetObjectGroupHandle GroupHandle);
	void SetGroupName(FNetObjectGroupHandle GroupHandle, FName GroupName);

	inline bool IsValidGroup(FNetObjectGroupHandle GroupHandle) const { return GroupHandle && Groups.IsValidIndex(GroupHandle); }

	bool Contains(FNetObjectGroupHandle GroupHandle, FInternalNetHandle InternalIndex) const;
	void AddToGroup(FNetObjectGroupHandle GroupHandle, FInternalNetHandle InternalIndex);
	void RemoveFromGroup(FNetObjectGroupHandle GroupHandle, FInternalNetHandle InternalIndex);

	// Returns how many groups the given handle is a member of
	uint32 GetNumGroupMemberships(FInternalNetHandle InternalIndex) const;
	const FNetObjectGroupHandle* GetGroupMemberships(FInternalNetHandle InternalIndex, uint32& GroupCount) const;

	// Create and manage named groups, only groups created as a named group will be findable by name
	FNetObjectGroupHandle CreateNamedGroup(FName GroupName);
	// Lookup NetObjectGroupHandle for a named group
	FNetObjectGroupHandle GetNamedGroupHandle(FName GroupName);
	// Destroy Named group
	void DestroyNamedGroup(FName GroupName);

private:
	struct FNetObjectGroupMembership
	{
		enum { MaxAssignedGroupCount = 4 };
		FNetObjectGroupHandle Groups[MaxAssignedGroupCount];		
	};

	static bool AddGroupMembership(FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group);
	static void RemoveGroupMembership(FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group);
	static void ResetGroupMembership(FNetObjectGroupMembership& Target);
	static bool IsMemberOf(const FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group);

	// Group usage pattern should not be high frequency so memory layout should not be a major concern
	TSparseArray<FNetObjectGroup> Groups;

	// Track what groups each internal handle is a member of, we can tighten this up a bit if needed
	TArray<FNetObjectGroupMembership> GroupMemberships;
	uint32 MaxGroupCount;

	TMap<FName, FNetObjectGroupHandle> NamedGroups;
};

}
