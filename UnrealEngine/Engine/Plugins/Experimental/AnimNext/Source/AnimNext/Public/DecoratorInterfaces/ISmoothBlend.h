// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/IDecoratorInterface.h"

namespace UE::AnimNext
{
	/**
	 * ISmoothBlend
	 *
	 * This interface exposes blend smoothing related information.
	 */
	struct ANIMNEXT_API ISmoothBlend : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(ISmoothBlend, 0x1c2c1739)

		// Returns the desired blend time for the specified child
		virtual float GetBlendTime(const FExecutionContext& Context, const TDecoratorBinding<ISmoothBlend>& Binding, int32 ChildIndex) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<ISmoothBlend> : FDecoratorBinding
	{
		// @see ISmoothBlend::GetBlendTime
		float GetBlendTime(const FExecutionContext& Context, int32 ChildIndex) const
		{
			return GetInterface()->GetBlendTime(Context, *this, ChildIndex);
		}

	protected:
		const ISmoothBlend* GetInterface() const { return GetInterfaceTyped<ISmoothBlend>(); }
	};
}
