// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/MemStack.h"
#include "DecoratorBase/IDecoratorInterface.h"

namespace UE::AnimNext
{
	// An array of children pointers
	// We reserve a small amount inline and spill on the memstack
	using FChildrenArray = TArray<FWeakDecoratorPtr, TInlineAllocator<8, TMemStackAllocator<>>>;

	/**
	 * IHierarchy
	 * 
	 * This interface exposes hierarchy traversal information to navigate the graph.
	 */
	struct ANIMNEXT_API IHierarchy : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IHierarchy, 0x846d8a37)

		// Returns the number of children
		// Includes inactive children
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const;

		// Appends weak handles to any children we wish to traverse.
		// Decorators are responsible for allocating and releasing child instance data.
		// Empty handles and duplicates can be appended.
		virtual void GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IHierarchy> : FDecoratorBinding
	{
		// @see IHierarchy::GetNumChildren
		uint32 GetNumChildren(const FExecutionContext& Context) const
		{
			return GetInterface()->GetNumChildren(Context, *this);
		}

		// @see IHierarchy::GetChildren
		void GetChildren(const FExecutionContext& Context, FChildrenArray& Children) const
		{
			GetInterface()->GetChildren(Context, *this, Children);
		}

	protected:
		const IHierarchy* GetInterface() const { return GetInterfaceTyped<IHierarchy>(); }
	};
}
