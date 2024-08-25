// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/SyncGroup_GraphInstanceComponent.h"
#include "DecoratorInterfaces/IGroupSynchronization.h"

namespace UE::AnimNext
{
	void FSyncGroupGraphInstanceComponent::RegisterWithGroup(FName GroupName, EAnimGroupRole::Type GroupRole, const FWeakDecoratorPtr& DecoratorPtr, const FDecoratorUpdateState& DecoratorState)
	{
		FSyncGroupState& GroupState = SyncGroupMap.FindOrAdd(GroupName);
		GroupState.Members.Add(FSyncGroupMember{ DecoratorState, DecoratorPtr, GroupRole });
	}

	void FSyncGroupGraphInstanceComponent::PreUpdate(FExecutionContext& Context)
	{
		// Reset our group state, we want to start fresh every update
		SyncGroupMap.Reset();
	}

	void FSyncGroupGraphInstanceComponent::PostUpdate(FExecutionContext& Context)
	{
		TDecoratorBinding<IGroupSynchronization> GroupSyncDecorator;

		// Now that we have discovered all groups and their memberships, we can perform synchronization
		for (const TTuple<FName, FSyncGroupState>& It : SyncGroupMap)
		{
			const FSyncGroupState& GroupState = It.Value;

			// Find our leader by looking at the total weight and the group role
			const int32 NumMembers = GroupState.Members.Num();
			check(NumMembers != 0);	// Groups should never be empty

			int32 LeaderIndex = INDEX_NONE;
			float LeaderTotalWeight = -1.0f;

			for (int32 MemberIndex = 0; MemberIndex < NumMembers; ++MemberIndex)
			{
				const FSyncGroupMember& GroupMember = GroupState.Members[MemberIndex];

				switch (GroupMember.GroupRole)
				{
				case EAnimGroupRole::CanBeLeader:
				case EAnimGroupRole::TransitionLeader:
					// Highest weight is the leader
					if (GroupMember.DecoratorState.GetTotalWeight() > LeaderTotalWeight)
					{
						LeaderIndex = MemberIndex;
						LeaderTotalWeight = GroupMember.DecoratorState.GetTotalWeight();
					}
					break;
				case EAnimGroupRole::AlwaysLeader:
					// Always set the leader index
					LeaderIndex = MemberIndex;
					LeaderTotalWeight = 2.0f;		// Some high value
					break;
				default:
				case EAnimGroupRole::AlwaysFollower:
				case EAnimGroupRole::TransitionFollower:
					// Never set the leader index
					// If we find no leader, we'll use the first index as set below
					break;
				}
			}

			if (LeaderIndex == INDEX_NONE)
			{
				// If none of the entries wish to be a leader, grab the first and force it
				LeaderIndex = 0;
			}

			// Now that we found our leader, advance it by the delta time
			float LeaderProgressRatio;
			{
				const FSyncGroupMember& GroupLeader = GroupState.Members[LeaderIndex];

				Context.BindTo(GroupLeader.DecoratorPtr);
				ensure(Context.GetInterface(GroupLeader.DecoratorPtr, GroupSyncDecorator));

				LeaderProgressRatio = GroupSyncDecorator.AdvanceBy(Context, GroupLeader.DecoratorState.GetDeltaTime());
			}

			// Advance every follower to the same progress ratio as the leader
			for (int32 MemberIndex = 0; MemberIndex < NumMembers; ++MemberIndex)
			{
				if (MemberIndex == LeaderIndex)
				{
					continue;	// Ignore the leader, it already advanced
				}

				const FSyncGroupMember& GroupMember = GroupState.Members[MemberIndex];

				Context.BindTo(GroupMember.DecoratorPtr);
				ensure(Context.GetInterface(GroupMember.DecoratorPtr, GroupSyncDecorator));

				GroupSyncDecorator.AdvanceToRatio(Context, LeaderProgressRatio);
			}
		}
	}
}
