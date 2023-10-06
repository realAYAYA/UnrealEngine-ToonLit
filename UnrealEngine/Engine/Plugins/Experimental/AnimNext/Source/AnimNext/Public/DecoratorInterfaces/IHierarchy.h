// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/MemStack.h"
#include "DecoratorBase/IDecoratorInterface.h"

namespace UE::AnimNext
{
	using FChildrenArray = TArray<FWeakDecoratorPtr, TMemStackAllocator<>>;

	/**
	 * IHierarchy
	 * 
	 * This interface exposes hierarchy traversal information to navigate the graph.
	 */
	struct ANIMNEXT_API IHierarchy : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IHierarchy, 0x846d8a37)

		// Appends weak handles to any children we wish to traverse.
		// Decorators are responsible for allocating and releasing child instance data.
		virtual void GetChildren(FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IHierarchy> : FDecoratorBinding
	{
		// @see IHierarchy::GetChildren
		void GetChildren(FExecutionContext& Context, FChildrenArray& Children) const
		{
			GetInterface()->GetChildren(Context, *this, Children);
		}

	protected:
		const IHierarchy* GetInterface() const { return GetInterfaceTyped<IHierarchy>(); }
	};
}
