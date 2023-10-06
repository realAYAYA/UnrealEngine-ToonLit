// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/IDecoratorInterface.h"
#include "DecoratorBase/ITraversalContext.h"

namespace UE::AnimNext
{
	/**
	 * FUpdateTraversalContext
	 * 
	 * Contains all relevant transient data for an update traversal.
	 */
	struct FUpdateTraversalContext : ITraversalContext
	{
		double GetDeltaTime() const { return 1.0 / 30.0; }
		double GetPlayRate() const { return 1.0; }
	};

	/**
	 * IUpdate
	 *
	 * This interface is called during the update traversal.
	 *
	 * When a node is decorator is visited, PreUpdate is first called on it. It is responsible for forwarding
	 * the call to the next decorator that implements this interface on the decorator stack of the node. Once
	 * all decorators have had the chance to PreUpdate, the children of the decorator are queried through
	 * the IHierarchy interface. The children will then evaluate and PostUpdate will then be called afterwards
	 * on the original decorator.
	 */
	struct ANIMNEXT_API IUpdate : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IUpdate, 0x59d24dc5)

		// Called before a decorator's children are updated
		virtual void PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const;

		// Called after a decorator's children have been updated
		virtual void PostUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IUpdate> : FDecoratorBinding
	{
		// @see IUpdate::PreUpdate
		void PreUpdate(FExecutionContext& Context) const
		{
			GetInterface()->PreUpdate(Context, *this);
		}

		// @see IUpdate::PostUpdate
		void PostUpdate(FExecutionContext& Context) const
		{
			GetInterface()->PostUpdate(Context, *this);
		}

	protected:
		const IUpdate* GetInterface() const { return GetInterfaceTyped<IUpdate>(); }
	};

	/**
	 * Updates a sub-graph starting at the graph root.
	 * Update starts at the top of the stack that includes the graph root decorator.
	 *
	 * For each node:
	 *     - We call PreUpdate on all its decorators
	 *     - We call GetChildren on all its decorators
	 *     - We update all children found
	 *     - We call PostUpdate on all its decorators
	 *
	 * @see IUpdate::PreUpdate, IUpdate::PostUpdate, IHierarchy::GetChildren
	 */
	ANIMNEXT_API void UpdateGraph(FExecutionContext& Context, FWeakDecoratorPtr GraphRootPtr);
}
