// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetObjectGroups.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/IConsoleManager.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h"
#include "Containers/ArrayView.h"

DEFINE_LOG_CATEGORY_STATIC(LogIrisGroup, Log, All)

namespace UE::Net::Private
{

static int32 CVarEnsureIfNumGroupMembershipsExceedsNum = 128;
static FAutoConsoleVariableRef CVarEnsureIfNumGroupMembershipsExceeds(TEXT("net.Iris.EnsureIfNumGroupMembershipsExceeds"), CVarEnsureIfNumGroupMembershipsExceedsNum, TEXT("If set to a positive number we will warn and ensure if an object is added to a high number of groups."), ECVF_Default );

FNetObjectGroups::FNetObjectGroups()
: CurrentEpoch(++NextEpoch)
{
}

FNetObjectGroups::~FNetObjectGroups()
{
}

bool FNetObjectGroups::IsMemberOf(const FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	return Target.Groups.Contains(Group);
}

bool FNetObjectGroups::AddGroupMembership(FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	if (!Target.Groups.Contains(Group))
	{
		Target.Groups.Add(Group);

		return true;
	}

	return false;
}

void FNetObjectGroups::RemoveGroupMembership(FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	Target.Groups.RemoveSingleSwap(Group);
}

void FNetObjectGroups::ResetGroupMembership(FNetObjectGroupMembership& Target)
{
	Target.Groups.Empty();
}

void FNetObjectGroups::Init(const FNetObjectGroupInitParams& Params)
{
	NetRefHandleManager = Params.NetRefHandleManager;

	ensureMsgf(Params.MaxGroupCount < std::numeric_limits<FNetObjectGroupHandle::FGroupIndexType>::max(), TEXT("MaxGroupCount cannot exceed %u"), std::numeric_limits<FNetObjectGroupHandle::FGroupIndexType>::max());
	MaxGroupCount = FMath::Clamp<uint32>(Params.MaxGroupCount, 0U, std::numeric_limits<FNetObjectGroupHandle::FGroupIndexType>::max());

	// Reserve first as invalid group
	Groups.Add(FNetObjectGroup());

	GroupMemberships.SetNum(Params.MaxObjectCount);
	GroupFilteredOutObjects.Init(Params.MaxObjectCount);
}

FNetObjectGroupHandle FNetObjectGroups::CreateGroup()
{
	if (ensure((uint32)Groups.Num() < MaxGroupCount))
	{
		const uint32 Index = static_cast<uint32>(Groups.Add(FNetObjectGroup()));
		FNetObjectGroupHandle GroupHandle;
		GroupHandle.Index = static_cast<FNetObjectGroupHandle::FGroupIndexType>(Index);
		GroupHandle.Epoch = CurrentEpoch;
		return GroupHandle;
	}
	else
	{
		return FNetObjectGroupHandle();
	}
}

void FNetObjectGroups::DestroyGroup(FNetObjectGroupHandle GroupHandle)
{
	if (IsValidGroup(GroupHandle))
	{
		ClearGroup(GroupHandle);

		FNetObjectGroup& Group = Groups[GroupHandle.GetGroupIndex()];

		if (EnumHasAnyFlags(Group.Traits, ENetObjectGroupTraits::IsFindableByName))
		{
			NamedGroups.FindAndRemoveChecked(Group.GroupName);
		}

		Groups.RemoveAt(GroupHandle.GetGroupIndex());
	}
}

void FNetObjectGroups::SetGroupName(FNetObjectGroupHandle GroupHandle, FName GroupName)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		if (ensureMsgf(EnumHasAnyFlags(Group->Traits, ENetObjectGroupTraits::IsFindableByName), TEXT("FNetObjectGroups::SetGroupName Cannot SetGroupName for grouphandle %u as it is FindableByName"), GroupHandle.GetGroupIndex()))
		{
			Group->GroupName = GroupName;
		}
	}
}

FName FNetObjectGroups::GetGroupName(FNetObjectGroupHandle GroupHandle) const
{
	if (const FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		return Group->GroupName;
	}

	return FName();
}

FNetObjectGroupHandle FNetObjectGroups::CreateNamedGroup(FName GroupName)
{
	if (NamedGroups.Contains(GroupName))
	{
		ensureMsgf(false, TEXT("FNetObjectGroups, trying to create named group %s that already exists"), *GroupName.ToString());
		return FNetObjectGroupHandle();
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

	return FNetObjectGroupHandle();
}

void FNetObjectGroups::DestroyNamedGroup(FName GroupName)
{
	DestroyGroup(GetNamedGroupHandle(GroupName));
}

void FNetObjectGroups::ClearGroup(FNetObjectGroupHandle GroupHandle)
{
	if (IsValidGroup(GroupHandle))
	{
		FNetObjectGroup& Group = Groups[GroupHandle.GetGroupIndex()];

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
	return IsValidGroup(GroupHandle) ? &Groups[GroupHandle.GetGroupIndex()] : nullptr;
}

FNetObjectGroup* FNetObjectGroups::GetGroup(FNetObjectGroupHandle GroupHandle)
{
	return IsValidGroup(GroupHandle) ? &Groups[GroupHandle.GetGroupIndex()] : nullptr;
}

const FNetObjectGroup* FNetObjectGroups::GetGroupByIndex(FNetObjectGroupHandle::FGroupIndexType GroupIndex) const
{
	return (GroupIndex != FNetObjectGroupHandle::InvalidNetObjectGroupIndex && Groups.IsValidIndex(GroupIndex)) ? &Groups[GroupIndex] : nullptr;
}

FNetObjectGroup* FNetObjectGroups::GetGroupByIndex(FNetObjectGroupHandle::FGroupIndexType GroupIndex)
{
	return (GroupIndex != FNetObjectGroupHandle::InvalidNetObjectGroupIndex && Groups.IsValidIndex(GroupIndex)) ? &Groups[GroupIndex] : nullptr;
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
		UE_LOG(LogIrisGroup, Verbose, TEXT("FNetObjectGroups::AddToGroup Adding %s to Group %s (GroupIndex: %u)"),
			*NetRefHandleManager->PrintObjectFromIndex(InternalIndex), *GetGroupName(GroupHandle).ToString(), GroupHandle.GetGroupIndex());

		FNetObjectGroupMembership& Membership = GroupMemberships[InternalIndex];
		if (AddGroupMembership(Membership, GroupHandle))
		{
			Group->Members.AddUnique(InternalIndex);

			if (IsFilterGroup(*Group))
			{
				GroupFilteredOutObjects.SetBit(InternalIndex);
			}

			if (CVarEnsureIfNumGroupMembershipsExceedsNum > 0 && Membership.Groups.Num() > CVarEnsureIfNumGroupMembershipsExceedsNum)
			{
				UE_LOG(LogIrisGroup, Error, TEXT("FNetObjectGroups::AddGroupMembership Unexpected high num groupmemberships for group %s (GroupIndex: %u) NetObject %s is member of %d groups."),
					*GetGroupName(GroupHandle).ToString(), GroupHandle.GetGroupIndex(), *NetRefHandleManager->PrintObjectFromIndex(InternalIndex), Membership.Groups.Num());
				ensure(false);			
			}
		}
	}
}

void FNetObjectGroups::RemoveFromGroup(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex InternalIndex)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (InternalIndex != FNetRefHandleManager::InvalidInternalIndex && Group)
	{
		UE_LOG(LogIrisGroup, Verbose, TEXT("FNetObjectGroups::RemoveFromGroup Removing %s from Group %s (GroupIndex: %u)"),
			*NetRefHandleManager->PrintObjectFromIndex(InternalIndex), *GetGroupName(GroupHandle).ToString(), GroupHandle.GetGroupIndex());

		FNetObjectGroupMembership& GroupMembership = GroupMemberships[InternalIndex];
		checkSlow(IsMemberOf(GroupMembership, GroupHandle));

		RemoveGroupMembership(GroupMembership, GroupHandle);
		Group->Members.RemoveSingle(InternalIndex);

		// Check to see if the object is still part of a filter group
		if (!IsInAnyFilterGroup(GroupMembership))
		{
			GroupFilteredOutObjects.ClearBit(InternalIndex);
		}
	}
}

void FNetObjectGroups::AddExclusionFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		if (!IsFilterGroup(*Group))
		{
			UE_LOG(LogIrisGroup, Verbose, TEXT("FNetObjectGroups::AddExclusionFilterTrait to Group %s (GroupIndex: %u)"), *GetGroupName(GroupHandle).ToString(), GroupHandle.GetGroupIndex());

			Group->Traits |= ENetObjectGroupTraits::IsExclusionFiltering;

			// Flag all current members of this group that they are now filterable
			for (FInternalNetRefIndex MemberIndex : Group->Members)
			{
				GroupFilteredOutObjects.SetBit(MemberIndex);
			}
		}
	}
}

void FNetObjectGroups::RemoveExclusionFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (Group == nullptr)
	{
		return;
	}

	if (!IsExclusionFilterGroup(*Group))
	{
		return;
	}

	UE_LOG(LogIrisGroup, Verbose, TEXT("FNetObjectGroups::RemoveExclusionFilterTrait to Group %s (GroupIndex: %u)"), *GetGroupName(GroupHandle).ToString(), GroupHandle.GetGroupIndex());

	Group->Traits &= ~(ENetObjectGroupTraits::IsExclusionFiltering);

	for (FInternalNetRefIndex MemberIndex : Group->Members)
	{
		// Check to see if the object is still part of a filter group
		if (!IsInAnyFilterGroup(GroupMemberships[MemberIndex]))
		{
			GroupFilteredOutObjects.ClearBit(MemberIndex);
		}
	}
}

void FNetObjectGroups::AddInclusionFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		// Can't be both inclusion and exclusion so let's do nothing if the group has any sort of filter trait.
		if (!IsFilterGroup(*Group))
		{
			Group->Traits |= ENetObjectGroupTraits::IsInclusionFiltering;
		}
	}
}

void FNetObjectGroups::RemoveInclusionFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (Group == nullptr)
	{
		return;
	}

	// Simply remove the trait.
	Group->Traits &= ~(ENetObjectGroupTraits::IsInclusionFiltering);
}

bool FNetObjectGroups::IsFilterGroup(FNetObjectGroupHandle GroupHandle) const
{
	if (const FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		return EnumHasAnyFlags(Group->Traits, ENetObjectGroupTraits::IsExclusionFiltering | ENetObjectGroupTraits::IsInclusionFiltering);
	}

	return false;
}

bool FNetObjectGroups::IsInAnyFilterGroup(const FNetObjectGroupMembership& GroupMembership) const
{
	for (FNetObjectGroupHandle AssignedGroup : GroupMembership.Groups)
	{
		if (!AssignedGroup.IsValid())
		{
			// Note: An invalid group means we found the end of the array
			return false;
		}
		else if (IsFilterGroup(Groups[AssignedGroup.GetGroupIndex()]))
		{
			return true;
		}
	}

	return false;
}

const FNetObjectGroupHandle* FNetObjectGroups::GetGroupMemberships(FInternalNetRefIndex InternalIndex, uint32& GroupCount) const
{
	if (InternalIndex >= (uint32)GroupMemberships.Num())
	{
		return nullptr;
	}

	const FNetObjectGroupMembership& MemberShip = GroupMemberships[InternalIndex];
	GroupCount = MemberShip.Groups.Num();
	return GroupCount ? MemberShip.Groups.GetData() : nullptr;
}

}
