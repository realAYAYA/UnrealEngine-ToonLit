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
		virtual double GetPlayRate(FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<ITimeline> : FDecoratorBinding
	{
		// @see ITimeline::GetPlayRate
		double GetPlayRate(FExecutionContext& Context) const
		{
			return GetInterface()->GetPlayRate(Context, *this);
		}

	protected:
		const ITimeline* GetInterface() const { return GetInterfaceTyped<ITimeline>(); }
	};
}
