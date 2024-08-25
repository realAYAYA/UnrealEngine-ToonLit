// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorBinding.h"
#include "DecoratorBase/DecoratorHandle.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorInterfaceUID.h"
#include "DecoratorBase/LatentPropertyHandle.h"
#include "DecoratorBase/NodeHandle.h"
#include "Graph/AnimNextGraphInstancePtr.h"

struct FAnimNextGraphInstance;

namespace UE::AnimNext
{
	struct FNodeDescription;
	struct FNodeInstance;
	struct FNodeTemplateRegistry;
	struct FNodeTemplate;
	struct FDecorator;
	struct FDecoratorRegistry;
	struct FDecoratorTemplate;

	/**
	 * Execution Context
	 * 
	 * The execution context aims to centralize the decorator query API.
	 * It is meant to be bound to a graph instance and re-used by the nodes/decorators within.
	 */
	struct ANIMNEXT_API FExecutionContext
	{
		// Creates an unbound execution context
		FExecutionContext();

		// Creates an execution context and binds it to the specified graph instance
		explicit FExecutionContext(FAnimNextGraphInstancePtr& InGraphInstance);

		// Creates an execution context and binds it to the specified graph instance
		explicit FExecutionContext(FAnimNextGraphInstance& InGraphInstance);

		// Binds the execution context to the specified graph instance if it differs from the currently bound instance
		void BindTo(FAnimNextGraphInstancePtr& InGraphInstance);

		// Binds the execution context to the specified graph instance if it differs from the currently bound instance
		void BindTo(FAnimNextGraphInstance& InGraphInstance);

		// Binds the execution context to the graph instance that owns the specified decorator if it differs from the currently bound instance
		void BindTo(const FWeakDecoratorPtr& DecoratorPtr);

		// Returns whether or not this execution context is bound to a graph instance
		bool IsBound() const;

		// Returns whether or not this execution context is bound to the specified graph instance
		bool IsBoundTo(const FAnimNextGraphInstancePtr& InGraphInstance) const;

		// Returns whether or not this execution context is bound to the specified graph instance
		bool IsBoundTo(const FAnimNextGraphInstance& InGraphInstance) const;

		// Queries a node for a decorator that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterface(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const;

		// Queries a node for a decorator that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterface(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const;

		// Queries a node for a decorator lower on the stack that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterfaceSuper(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& SuperBinding) const;

		// Queries a node for a decorator lower on the stack that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterfaceSuper(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& SuperBinding) const;

		// Allocates a new node instance from a decorator handle
		// If the desired decorator lives in the current parent, a weak handle to it will be returned
		FDecoratorPtr AllocateNodeInstance(const FDecoratorBinding& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const;

		// Allocates a new node instance from a decorator handle
		// If the desired decorator lives in the current parent, a weak handle to it will be returned
		FDecoratorPtr AllocateNodeInstance(const FWeakDecoratorPtr& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const;

		// Decrements the reference count of the provided node pointer and releases it if
		// there are no more references remaining, reseting the pointer in the process
		void ReleaseNodeInstance(FDecoratorPtr& NodePtr) const;

		// Takes a snapshot of all latent properties on the provided node sub-stack (all decorators on the sub-stack of the provided one)
		// Properties can be marked as always updating or as supporting freezing (e.g. when a branch of the graph blends out)
		// A freezable property does not update when a snapshot is taken of a frozen node
		void SnapshotLatentProperties(const FWeakDecoratorPtr& DecoratorPtr, bool bIsFrozen) const;

		// Returns a typed graph instance component, creating it lazily the first time it is queried
		template<class ComponentType>
		ComponentType& GetComponent() const;

		// Returns a typed graph instance component pointer if found or nullptr otherwise
		template<class ComponentType>
		ComponentType* TryGetComponent() const;

		// Returns const iterators to the graph instance component container
		GraphInstanceComponentMapType::TConstIterator GetComponentIterator() const;

		// Returns the bound graph instance
		FAnimNextGraphInstance& GetGraphInstance() const { return *GraphInstance; }

	private:
		// No copy or move
		FExecutionContext(const FExecutionContext&) = delete;
		FExecutionContext& operator=(const FExecutionContext&) = delete;

		bool GetInterfaceImpl(FDecoratorInterfaceUID InterfaceUID, const FWeakDecoratorPtr& DecoratorPtr, FDecoratorBinding& InterfaceBinding) const;
		bool GetInterfaceSuperImpl(FDecoratorInterfaceUID InterfaceUID, const FWeakDecoratorPtr& DecoratorPtr, FDecoratorBinding& SuperBinding) const;
		FGraphInstanceComponent* TryGetComponent(int32 ComponentNameHash, FName ComponentName) const;
		FGraphInstanceComponent& AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<FGraphInstanceComponent>&& Component) const;

		const FNodeDescription& GetNodeDescription(FNodeHandle NodeHandle) const;
		const FNodeTemplate* GetNodeTemplate(const FNodeDescription& NodeDesc) const;
		const FDecorator* GetDecorator(const FDecoratorTemplate& DecoratorDesc) const;

		// Cached references to the registries we need
		const FNodeTemplateRegistry& NodeTemplateRegistry;
		const FDecoratorRegistry& DecoratorRegistry;

		// Cached properties for the currently executing graph
		FAnimNextGraphInstance* GraphInstance = nullptr;
		TArrayView<const uint8> GraphSharedData;
	};

	//////////////////////////////////////////////////////////////////////////
	// Inline implementations

	template<class DecoratorInterface>
	inline bool FExecutionContext::GetInterface(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const
	{
		constexpr FDecoratorInterfaceUID InterfaceUID = DecoratorInterface::InterfaceUID;
		return GetInterfaceImpl(InterfaceUID, DecoratorPtr, InterfaceBinding);
	}

	template<class DecoratorInterface>
	inline bool FExecutionContext::GetInterface(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const
	{
		return GetInterface<DecoratorInterface>(Binding.GetDecoratorPtr(), InterfaceBinding);
	}

	template<class DecoratorInterface>
	inline bool FExecutionContext::GetInterfaceSuper(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& SuperBinding) const
	{
		constexpr FDecoratorInterfaceUID InterfaceUID = DecoratorInterface::InterfaceUID;
		return GetInterfaceSuperImpl(InterfaceUID, DecoratorPtr, SuperBinding);
	}

	template<class DecoratorInterface>
	inline bool FExecutionContext::GetInterfaceSuper(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& SuperBinding) const
	{
		return GetInterfaceSuper<DecoratorInterface>(Binding.GetDecoratorPtr(), SuperBinding);
	}

	inline FDecoratorPtr FExecutionContext::AllocateNodeInstance(const FDecoratorBinding& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const
	{
		return AllocateNodeInstance(ParentBinding.GetDecoratorPtr(), ChildDecoratorHandle);
	}

	template<class ComponentType>
	ComponentType& FExecutionContext::GetComponent() const
	{
		const FName ComponentName = ComponentType::StaticComponentName();
		const int32 ComponentNameHash = GetTypeHash(ComponentName);

		if (FGraphInstanceComponent* Component = TryGetComponent(ComponentNameHash, ComponentName))
		{
			return *static_cast<ComponentType*>(Component);
		}

		return static_cast<ComponentType&>(AddComponent(ComponentNameHash, ComponentName, MakeShared<ComponentType>()));
	}

	template<class ComponentType>
	ComponentType* FExecutionContext::TryGetComponent() const
	{
		const FName ComponentName = ComponentType::StaticComponentName();
		const int32 ComponentNameHash = GetTypeHash(ComponentName);

		return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
	}
}
