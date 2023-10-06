// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetObjectGroups.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h" // for InvalidInternalIndex
#include "Containers/ArrayView.h"

namespace UE::Net::Private
{

FNetObjectGroups::FNetObjectGroups()
: MaxGroupCount(0U)
{
}

FNetObjectGroups::~FNetObjectGroups()
{
}

bool FNetObjectGroups::IsMemberOf(const FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	for (uint32 It = 0; It < FNetObjectGroupMembership::MaxAssignedGroupCount; ++It)
	{
		if (Target.Groups[It] == Group)
		{
			return true;
		}
	}

	return false;
}

bool FNetObjectGroups::AddGroupMembership(FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	for (uint32 It = 0; It < FNetObjectGroupMembership::MaxAssignedGroupCount; ++It)
	{
		if (Target.Groups[It] == Group)
		{
			return false;
		}
		else if (Target.Groups[It] == InvalidNetObjectGroupHandle)
		{
			Target.Groups[It] = Group;
			return true;
		}
	}

	return false;
}

void FNetObjectGroups::RemoveGroupMembership(FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	const uint32 LastIndex = FNetObjectGroupMembership::MaxAssignedGroupCount - 1U;
	for (uint32 It = 0U; It < FNetObjectGroupMembership::MaxAssignedGroupCount; ++It)
	{
		if (Target.Groups[It] == Group)
		{
			FPlatformMemory::Memmove(&Target.Groups[It], &Target.Groups[It + 1], (LastIndex - It) * sizeof(FNetObjectGroupHandle));
			Target.Groups[LastIndex] = InvalidNetObjectGroupHandle;

			return;
		}
	}
}

void FNetObjectGroups::ResetGroupMembership(FNetObjectGroupMembership& Target)
{
	FMemory::Memzero(Target);
}

void FNetObjectGroups::Init(const FNetObjectGroupInitParams& Params)
{
	MaxGroupCount = Params.MaxGroupCount;

	// Reserve first as invalid group
	Groups.Add(FNetObjectGroup());

	GroupMemberships.SetNumZeroed(Params.MaxObjectCount);
	GroupFilteredObjects.Init(Params.MaxObjectCount);
}

FNetObjectGroupHandle FNetObjectGroups::CreateGroup()
{
	if (ensure((uint32)Groups.Num() < MaxGroupCount))
	{
		const int Index = Groups.Add(FNetObjectGroup());
		return FNetObjectGroupHandle(Index);
	}
	else
	{
		return InvalidNetObjectGroupHandle;
	}
}

void FNetObjectGroups::DestroyGroup(FNetObjectGroupHandle GroupHandle)
{
	if (GroupHandle != InvalidNetObjectGroupHandle && Groups.IsValidIndex(GroupHandle))
	{
		ClearGroup(GroupHandle);

		FNetObjectGroup& Group = Groups[GroupHandle];

		if (EnumHasAnyFlags(Group.Traits, ENetObjectGroupTraits::IsFindableByName))
		{
			NamedGroups.FindAndRemoveChecked(Group.GroupName);
		}

		Groups.RemoveAt(GroupHandle);
	}
}

void FNetObjectGroups::SetGroupName(FNetObjectGroupHandle GroupHandle, FName GroupName)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		if (ensureAlwaysMsgf(EnumHasAnyFlags(Group->Traits, ENetObjectGroupTraits::IsFindableByName), TEXT("FNetObjectGroups::SetGroupName Cannot SetGroupName for grouphandle %u as it is FindableByName"), GroupHandle))
		{
			Group->GroupName = GroupName;
		}
	}
}

FNetObjectGroupHandle FNetObjectGroups::CreateNamedGroup(FName GroupName)
{
	if (NamedGroups.Contains(GroupName))
	{
		ensureAlwaysMsgf(false, TEXT("FNetObjectGroups, trying to create named group %s that already exists"), *GroupName.ToString());
		return InvalidNetObjectGroupHandle;
	}

	FNetObjectGroupHandle GroupHandle = CreateGroup();
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		Group->GroupName = GroupName;
		Group->Traits |= ENetObjectGroupTraits::IsFindableByName;
		NamedGroups.Add(GroupName, GroupHandle);
	}

	return GroupHandle;
}

FNetObjectGroupHandle FNetObjectGroups::GetNamedGroupHandle(FName GroupName)
{
	if (FNetObjectGroupHandle* Group = NamedGroups.Find(GroupName))
	{
		return *Group;
	}

	return InvalidNetObjectGroupHandle;
}

void FNetObjectGroups::DestroyNamedGroup(FName GroupName)
{
	DestroyGroup(GetNamedGroupHandle(GroupName));
}

void FNetObjectGroups::ClearGroup(FNetObjectGroupHandle GroupHandle)
{
	if (GroupHandle != InvalidNetObjectGroupHandle && Groups.IsValidIndex(GroupHandle))
	{
		FNetObjectGroup& Group = Groups[GroupHandle];

		for (FInternalNetRefIndex InternalIndex : Group.Members)
		{
			checkSlow(IsMemberOf(GroupMemberships[InternalIndex], GroupHandle));
			RemoveGroupMembership(GroupMemberships[InternalIndex], GroupHandle);
		}

		Group.Members.Empty();
	}
}

const FNetObjectGroup* FNetObjectGroups::GetGroup(FNetObjectGroupHandle GroupHandle) const
{
	return Groups.IsValidIndex(GroupHandle) ? &Groups[GroupHandle] : nullptr;
}

FNetObjectGroup* FNetObjectGroups::GetGroup(FNetObjectGroupHandle GroupHandle)
{
	return Groups.IsValidIndex(GroupHandle) ? &Groups[GroupHandle] : nullptr;
}

bool FNetObjectGroups::Contains(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex InternalIndex) const
{
	// If the group does not exist we cannot be in it..
	const FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (!Group)
	{
		return false;
	}

	checkSlow(InternalIndex == 0U || (Group->Members.Contains(InternalIndex) == IsMemberOf(GroupMemberships[InternalIndex], GroupHandle)));

	return InternalIndex ? IsMemberOf(GroupMemberships[InternalIndex], GroupHandle) : false;
}

void FNetObjectGroups::AddToGroup(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex InternalIndex)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (InternalIndex != FNetRefHandleManager::InvalidInternalIndex && Group)
	{
		if (AddGroupMembership(GroupMemberships[InternalIndex], GroupHandle))
		{
			Group->Members.AddUnique(InternalIndex);

			if (IsFilterGroup(*Group))
			{
				GroupFilteredObjects.SetBit(InternalIndex);
			}
		}
		else
		{
			checkf(false, TEXT("FNetObjectGroups::AddToGroup, Failed to add ( InternalIndex: %u ) to (GroupIndex: %u) A NetObject can only be a member of %u groups."), InternalIndex, GroupHandle, FNetObjectGroupMembership::MaxAssignedGroupCount);
		}
	}
}

void FNetObjectGroups::RemoveFromGroup(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex InternalIndex)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (InternalIndex != FNetRefHandleManager::InvalidInternalIndex && Group)
	{
		FNetObjectGroupMembership& GroupMembership = GroupMemberships[InternalIndex];
		checkSlow(IsMemberOf(GroupMembership, GroupHandle));

		RemoveGroupMembership(GroupMembership, GroupHandle);
		Group->Members.RemoveSingle(InternalIndex);

		// Check to see if the object is still part of a filter group
		if (!IsInAnyFilterGroup(GroupMembership))
		{
			GroupFilteredObjects.ClearBit(InternalIndex);
		}
	}
}

void FNetObjectGroups::AddFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		if (!IsFilterGroup(*Group))
		{
			Group->Traits |= ENetObjectGroupTraits::IsFiltering;

			// Flag all current members of this group that they are now filterable
			for (FInternalNetRefIndex MemberIndex : Group->Members)
			{
				GroupFilteredObjects.SetBit(MemberIndex);
			}
		}
	}
}

void FNetObjectGroups::RemoveFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		Group->Traits &= ~(ENetObjectGroupTraits::IsFiltering);

		for (FInternalNetRefIndex MemberIndex : Group->Members)
		{
			// Check to see if the object is still part of a filter group
			if (!IsInAnyFilterGroup(GroupMemberships[MemberIndex]))
			{
				GroupFilteredObjects.ClearBit(MemberIndex);
			}
		}
	}
}

bool FNetObjectGroups::IsFilterGroup(FNetObjectGroupHandle GroupHandle) const
{
	if (const FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		return EnumHasAnyFlags(Group->Traits, ENetObjectGroupTraits::IsFiltering);
	}

	return false;
}

bool FNetObjectGroups::IsInAnyFilterGroup(const FNetObjectGroupMembership& GroupMembership) const
{
	for (FNetObjectGroupHandle AssignedGroup : GroupMembership.Groups)
	{
		if (AssignedGroup == InvalidNetObjectGroupHandle)
		{
			// Note: An invalid group means we found the end of the array
			return false;
		}
		else if (IsFilterGroup(Groups[AssignedGroup]))
		{
			return true;
		}
	}

	return false;
}

uint32 FNetObjectGroups::GetNumGroupMemberships(FInternalNetRefIndex InternalIndex) const
{
	if (InternalIndex >= (uint32)GroupMemberships.Num())
	{
		return 0U;
	}

	uint32 Count = 0U;
	for (uint32 It=0; It < FNetObjectGroupMembership::MaxAssignedGroupCount; ++It)
	{
		Count += GroupMemberships[InternalIndex].Groups[It] != 0U;
	}
	
	return Count;
}

const FNetObjectGroupHandle* FNetObjectGroups::GetGroupMemberships(FInternalNetRefIndex InternalIndex, uint32& GroupCount) const
{
	GroupCount = GetNumGroupMemberships(InternalIndex);
	if (GroupCount)
	{
		return GroupMemberships[InternalIndex].Groups;
	}
	else
	{
		return nullptr;
	}
}

}
