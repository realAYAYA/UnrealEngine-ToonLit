// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/IDecoratorInterface.h"

namespace UE::AnimNext
{
	/**
	 * ITimeline
	 *
	 * This interface exposes timeline related information.
	 */
	struct ANIMNEXT_API ITimeline : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(ITimeline, 0x53760727)

		// Returns the play rate of this timeline
		virtual float GetPlayRate(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const;

		// Advances time by the provided delta time (positive or negative) on this timeline
		// Returns the progress ratio of playback: 0.0 = start of animation, 1.0 = end of animation
		virtual float AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float DeltaTime) const;

		// Advances time to the specified progress ratio on this timeline
		// Progress ratio must be between [0.0, 1.0]
		virtual void AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float ProgressRatio) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<ITimeline> : FDecoratorBinding
	{
		// @see ITimeline::GetPlayRate
		float GetPlayRate(const FExecutionContext& Context) const
		{
			return GetInterface()->GetPlayRate(Context, *this);
		}

		// @see ITimeline::AdvanceBy
		float AdvanceBy(const FExecutionContext& Context, float DeltaTime) const
		{
			return GetInterface()->AdvanceBy(Context, *this, DeltaTime);
		}

		// @see ITimeline::AdvanceToRatio
		void AdvanceToRatio(const FExecutionContext& Context, float ProgressRatio) const
		{
			GetInterface()->AdvanceToRatio(Context, *this, ProgressRatio);
		}

	protected:
		const ITimeline* GetInterface() const { return GetInterfaceTyped<ITimeline>(); }
	};
}
