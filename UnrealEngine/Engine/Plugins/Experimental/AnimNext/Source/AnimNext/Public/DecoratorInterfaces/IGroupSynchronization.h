// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimationAsset.h"
#include "DecoratorBase/IDecoratorInterface.h"

namespace UE::AnimNext
{
	/**
	 * IGroupSynchronization
	 *
	 * This interface exposes group synchronization related information and behavior.
	 */
	struct ANIMNEXT_API IGroupSynchronization : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IGroupSynchronization, 0xf607d0fd)

		// Returns the group name used for synchronization
		virtual FName GetGroupName(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding) const;

		// Returns the group role used for synchronization
		virtual EAnimGroupRole::Type GetGroupRole(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding) const;

		// Called by the sync group graph instance component once a group has been synchronized to advance time on the leader
		// Returns the progress ratio of playback: 0.0 = start of animation, 1.0 = end of animation
		virtual float AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding, float DeltaTime) const;

		// Called by the sync group graph instance component once a group has been synchronized to advance time on each follower
		// Progress ratio must be between [0.0, 1.0]
		virtual void AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding, float ProgressRatio) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IGroupSynchronization> : FDecoratorBinding
	{
		// @see IGroupSynchronization::GetGroupName
		FName GetGroupName(const FExecutionContext& Context) const
		{
			return GetInterface()->GetGroupName(Context, *this);
		}

		// @see IGroupSynchronization::GetGroupRole
		EAnimGroupRole::Type GetGroupRole(const FExecutionContext& Context) const
		{
			return GetInterface()->GetGroupRole(Context, *this);
		}

		// @see IGroupSynchronization::AdvanceBy
		float AdvanceBy(const FExecutionContext& Context, float DeltaTime) const
		{
			return GetInterface()->AdvanceBy(Context, *this, DeltaTime);
		}

		// @see IGroupSynchronization::AdvanceToRatio
		void AdvanceToRatio(const FExecutionContext& Context, float ProgressRatio) const
		{
			GetInterface()->AdvanceToRatio(Context, *this, ProgressRatio);
		}

	protected:
		const IGroupSynchronization* GetInterface() const { return GetInterfaceTyped<IGroupSynchronization>(); }
	};
}
