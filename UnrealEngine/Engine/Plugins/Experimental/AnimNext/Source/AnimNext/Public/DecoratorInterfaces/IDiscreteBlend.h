// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/IDecoratorInterface.h"

struct FAlphaBlend;

namespace UE::AnimNext
{
	/**
	 * IDiscreteBlend
	 *
	 * This interface exposes discrete blend related information.
	 */
	struct ANIMNEXT_API IDiscreteBlend : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IDiscreteBlend, 0x2d395d56)

		// Returns the blend weight for the specified child
		// Multiple children can have non-zero weight but their sum must be 1.0
		// Returns -1.0 if the child index is invalid
		virtual float GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const;

		// Returns the blend alpha for the specified child
		// Returns nullptr if the child index is invalid
		// Allows additive decorators to query the internal state of the base decorator
		virtual const FAlphaBlend* GetBlendState(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const;

		// Returns the blend destination child index (aka the active child index)
		// Returns INDEX_NONE if no child is active
		virtual int32 GetBlendDestinationChildIndex(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const;

		// Called when a blend transition between children occurs
		// OldChildIndex can be INDEX_NONE if there was no previously active child
		// NewChildIndex can be larger than the current number of known children to support a dynamic number of children at runtime
		// When this occurs, the number of children increments by one
		// The number of children never shrinks
		virtual void OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const;

		// Called when the blend for specified child is initiated
		virtual void OnBlendInitiated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const;

		// Called when the blend for specified child terminates
		virtual void OnBlendTerminated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IDiscreteBlend> : FDecoratorBinding
	{
		// @see IDiscreteBlend::GetBlendWeight
		float GetBlendWeight(const FExecutionContext& Context, int32 ChildIndex) const
		{
			return GetInterface()->GetBlendWeight(Context, *this, ChildIndex);
		}

		// @see IDiscreteBlend::GetBlendState
		const FAlphaBlend* GetBlendState(const FExecutionContext& Context, int32 ChildIndex) const
		{
			return GetInterface()->GetBlendState(Context, *this, ChildIndex);
		}

		// @see IDiscreteBlend::GetBlendDestinationChildIndex
		int32 GetBlendDestinationChildIndex(const FExecutionContext& Context) const
		{
			return GetInterface()->GetBlendDestinationChildIndex(Context, *this);
		}

		// @see IDiscreteBlend::OnBlendTransition
		void OnBlendTransition(const FExecutionContext& Context, int32 OldChildIndex, int32 NewChildIndex) const
		{
			GetInterface()->OnBlendTransition(Context, *this, OldChildIndex, NewChildIndex);
		}

		// @see IDiscreteBlend::OnBlendInitiated
		void OnBlendInitiated(const FExecutionContext& Context, int32 ChildIndex) const
		{
			GetInterface()->OnBlendInitiated(Context, *this, ChildIndex);
		}

		// @see IDiscreteBlend::OnBlendTerminated
		void OnBlendTerminated(const FExecutionContext& Context, int32 ChildIndex) const
		{
			GetInterface()->OnBlendTerminated(Context, *this, ChildIndex);
		}

	protected:
		const IDiscreteBlend* GetInterface() const { return GetInterfaceTyped<IDiscreteBlend>(); }
	};
}
