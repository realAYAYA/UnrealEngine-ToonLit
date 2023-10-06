// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorBinding.h"
#include "DecoratorBase/DecoratorHandle.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorInterfaceUID.h"
#include "DecoratorBase/NodeHandle.h"

namespace UE::AnimNext
{
	struct FNodeDescription;
	struct FNodeInstance;
	struct FNodeTemplate;
	struct FDecorator;
	struct FDecoratorTemplate;
	struct ITraversalContext;

	/**
	 * Execution Context
	 * 
	 * The execution context holds internal state during traversals of the animation graph.
	 */
	struct ANIMNEXT_API FExecutionContext
	{
		// Creates an execution context for the specified graph
		explicit FExecutionContext(TArrayView<const uint8> GraphSharedData);

		// Destroys the execution context
		~FExecutionContext();

		// Queries a node for a decorator that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterface(FWeakDecoratorPtr DecoratorPtr, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const;

		// Queries a node for a decorator that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterface(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const;

		// Queries a node for a decorator lower on the stack that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterfaceSuper(FWeakDecoratorPtr DecoratorPtr, TDecoratorBinding<DecoratorInterface>& SuperBinding) const;

		// Queries a node for a decorator lower on the stack that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterfaceSuper(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& SuperBinding) const;

		// Allocates a new node instance from a decorator handle
		// If the desired decorator lives in the current parent, a weak handle to it will be returned
		FDecoratorPtr AllocateNodeInstance(const FDecoratorBinding& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle);

		// Allocates a new node instance from a decorator handle
		// If the desired decorator lives in the current parent, a weak handle to it will be returned
		FDecoratorPtr AllocateNodeInstance(FWeakDecoratorPtr ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle);

		// Releases a node instance that is no longer referenced
		void ReleaseNodeInstance(FNodeInstance* Node);

		// Returns the current strongly typed traversal context or nullptr if not in a traversal
		template<class TraversalContextType>
		TraversalContextType& GetTraversalContext() const { return *static_cast<TraversalContextType*>(TraversalContext); }	// TODO: Add a casting check for safety

	private:
		FExecutionContext(const FExecutionContext&) = delete;
		FExecutionContext(FExecutionContext&&) = delete;

		bool GetInterfaceImpl(FDecoratorInterfaceUID InterfaceUID, FWeakDecoratorPtr DecoratorPtr, FDecoratorBinding& InterfaceBinding) const;
		bool GetInterfaceSuperImpl(FDecoratorInterfaceUID InterfaceUID, FWeakDecoratorPtr DecoratorPtr, FDecoratorBinding& SuperBinding) const;

		const FNodeDescription& GetNodeDescription(FNodeHandle NodeHandle) const;
		const FNodeTemplate* GetNodeTemplate(const FNodeDescription& NodeDesc) const;
		const FDecorator* GetDecorator(const FDecoratorTemplate& DecoratorDesc) const;

		ITraversalContext* TraversalContext = nullptr;

		TArrayView<const uint8> GraphSharedData;

		friend struct FScopedTraversalContext;
	};

	// Returns a pointer to the current execution context if present, nullptr otherwise.
	FExecutionContext* GetThreadExecutionContext();

	//////////////////////////////////////////////////////////////////////////

	template<class DecoratorInterface>
	bool FExecutionContext::GetInterface(FWeakDecoratorPtr DecoratorPtr, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const
	{
		constexpr FDecoratorInterfaceUID InterfaceUID = DecoratorInterface::InterfaceUID;
		return GetInterfaceImpl(InterfaceUID, DecoratorPtr, InterfaceBinding);
	}

	template<class DecoratorInterface>
	bool FExecutionContext::GetInterface(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const
	{
		return GetInterface<DecoratorInterface>(Binding.GetDecoratorPtr(), InterfaceBinding);
	}

	template<class DecoratorInterface>
	bool FExecutionContext::GetInterfaceSuper(FWeakDecoratorPtr DecoratorPtr, TDecoratorBinding<DecoratorInterface>& SuperBinding) const
	{
		constexpr FDecoratorInterfaceUID InterfaceUID = DecoratorInterface::InterfaceUID;
		return GetInterfaceSuperImpl(InterfaceUID, DecoratorPtr, SuperBinding);
	}

	template<class DecoratorInterface>
	bool FExecutionContext::GetInterfaceSuper(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& SuperBinding) const
	{
		return GetInterfaceSuper<DecoratorInterface>(Binding.GetDecoratorPtr(), SuperBinding);
	}

	inline FDecoratorPtr FExecutionContext::AllocateNodeInstance(const FDecoratorBinding& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle)
	{
		return AllocateNodeInstance(ParentBinding.GetDecoratorPtr(), ChildDecoratorHandle);
	}
}
