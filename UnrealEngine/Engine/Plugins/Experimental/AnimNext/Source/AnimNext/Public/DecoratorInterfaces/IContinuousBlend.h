// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/IDecoratorInterface.h"

namespace UE::AnimNext
{
	/**
	 * IContinuousBlend
	 *
	 * This interface exposes continuous blend related information.
	 */
	struct ANIMNEXT_API IContinuousBlend : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IContinuousBlend, 0xe7d79186)

		// Returns the blend weight for the specified child
		// Multiple children can have non-zero weight but their sum must be 1.0
		// Returns -1.0 if the child index is invalid
		virtual float GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IContinuousBlend>& Binding, int32 ChildIndex) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IContinuousBlend> : FDecoratorBinding
	{
		// @see IContinuousBlend::GetBlendWeight
		float GetBlendWeight(const FExecutionContext& Context, int32 ChildIndex) const
		{
			return GetInterface()->GetBlendWeight(Context, *this, ChildIndex);
		}

	protected:
		const IContinuousBlend* GetInterface() const { return GetInterfaceTyped<IContinuousBlend>(); }
	};
}
