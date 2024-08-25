// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	/**
	 * FExecutionContextProxy
	 *
	 * Wraps an execution context and forwards all calls to it by mirroring its API.
	 */
	struct FExecutionContextProxy
	{
		explicit FExecutionContextProxy(const FExecutionContext& InExecutionContext)
			: ExecutionContext(InExecutionContext)
		{
		}

		// We safely coerce to our wrapped execution context to allow identical usage
		operator const FExecutionContext& () const
		{
			return ExecutionContext;
		}

		// Returns whether or not this execution context is bound to a graph instance
		bool IsBound() const
		{
			return ExecutionContext.IsBound();
		}

		// Returns whether or not this execution context is bound to the specified graph instance
		bool IsBoundTo(const FAnimNextGraphInstancePtr& InGraphInstance) const
		{
			return ExecutionContext.IsBoundTo(InGraphInstance);
		}

		// Returns whether or not this execution context is bound to the specified graph instance
		bool IsBoundTo(const FAnimNextGraphInstance& InGraphInstance) const
		{
			return ExecutionContext.IsBoundTo(InGraphInstance);
		}

		// Queries a node for a decorator that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterface(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const
		{
			return ExecutionContext.GetInterface<DecoratorInterface>(DecoratorPtr, InterfaceBinding);
		}

		// Queries a node for a decorator that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterface(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const
		{
			return ExecutionContext.GetInterface<DecoratorInterface>(Binding, InterfaceBinding);
		}

		// Queries a node for a decorator lower on the stack that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterfaceSuper(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& SuperBinding) const
		{
			return ExecutionContext.GetInterfaceSuper<DecoratorInterface>(DecoratorPtr, SuperBinding);
		}

		// Queries a node for a decorator lower on the stack that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterfaceSuper(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& SuperBinding) const
		{
			return ExecutionContext.GetInterfaceSuper<DecoratorInterface>(Binding, SuperBinding);
		}

		// Allocates a new node instance from a decorator handle
		// If the desired decorator lives in the current parent, a weak handle to it will be returned
		FDecoratorPtr AllocateNodeInstance(const FDecoratorBinding& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const
		{
			return ExecutionContext.AllocateNodeInstance(ParentBinding, ChildDecoratorHandle);
		}

		// Allocates a new node instance from a decorator handle
		// If the desired decorator lives in the current parent, a weak handle to it will be returned
		FDecoratorPtr AllocateNodeInstance(const FWeakDecoratorPtr& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const
		{
			return ExecutionContext.AllocateNodeInstance(ParentBinding, ChildDecoratorHandle);
		}

		// Decrements the reference count of the provided node pointer and releases it if
		// there are no more references remaining, reseting the pointer in the process
		void ReleaseNodeInstance(FDecoratorPtr& NodePtr) const
		{
			return ExecutionContext.ReleaseNodeInstance(NodePtr);
		}

		// Takes a snapshot of all latent properties on the provided node sub-stack (all decorators on the sub-stack of the provided one)
		// Properties can be marked as always updating or as supporting freezing (e.g. when a branch of the graph blends out)
		// A freezable property does not update when a snapshot is taken of a frozen node
		void SnapshotLatentProperties(const FWeakDecoratorPtr& DecoratorPtr, bool bIsFrozen) const
		{
			ExecutionContext.SnapshotLatentProperties(DecoratorPtr, bIsFrozen);
		}

		// Returns a typed graph instance component, creating it lazily the first time it is queried
		template<class ComponentType>
		ComponentType& GetComponent() const
		{
			return ExecutionContext.GetComponent<ComponentType>();
		}

		// Returns a typed graph instance component pointer if found or nullptr otherwise
		template<class ComponentType>
		ComponentType* TryGetComponent() const
		{
			return ExecutionContext.TryGetComponent<ComponentType>();
		}

		// Returns const iterators to the graph instance component container
		GraphInstanceComponentMapType::TConstIterator GetComponentIterator() const
		{
			return ExecutionContext.GetComponentIterator();
		}

		// Returns the bound graph instance
		FAnimNextGraphInstance& GetGraphInstance() const
		{
			return ExecutionContext.GetGraphInstance();
		}

	private:
		// The execution context that we wrap
		const FExecutionContext& ExecutionContext;
	};
}
