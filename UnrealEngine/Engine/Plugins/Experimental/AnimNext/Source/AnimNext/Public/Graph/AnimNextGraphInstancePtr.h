// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorPtr.h"

#include "AnimNextGraphInstancePtr.generated.h"

class FReferenceCollector;

struct FAnimNextGraphInstance;
struct FRigUnit_AnimNextGraphEvaluator;
class UAnimNextGraph;

namespace UE::AnimNext
{
	struct FExecutionContext;
	struct FGraphInstanceComponent;
}

using GraphInstanceComponentMapType = TMap<FName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>>;

// Represents an instance of an AnimNext graph
// This struct uses UE reflection because we wish for the GC to keep the graph
// alive while we own a reference to it. It is not intended to be serialized on disk with a live instance.
USTRUCT()
struct ANIMNEXT_API FAnimNextGraphInstancePtr
{
	GENERATED_BODY()

	// Creates an empty graph instance that doesn't reference anything
	FAnimNextGraphInstancePtr();

	// No copying, only moving
	FAnimNextGraphInstancePtr(const FAnimNextGraphInstancePtr&) = delete;
	FAnimNextGraphInstancePtr& operator=(const FAnimNextGraphInstancePtr&) = delete;
	FAnimNextGraphInstancePtr(FAnimNextGraphInstancePtr&&);
	FAnimNextGraphInstancePtr& operator=(FAnimNextGraphInstancePtr&&);

	// If the graph instance is allocated, we release it during destruction
	~FAnimNextGraphInstancePtr();

	// Releases the graph instance and frees all corresponding memory
	void Release();

	// Returns true if we have a live graph instance, false otherwise
	bool IsValid() const;

	// Returns the graph used by this instance or nullptr if the instance is invalid
	const UAnimNextGraph* GetGraph() const;

	// Returns a weak handle to the root decorator instance
	UE::AnimNext::FWeakDecoratorPtr GetGraphRootPtr() const;

	// Returns the graph instance implementation
	FAnimNextGraphInstance* GetImpl() const;

	// Check to see if this instance data matches the provided graph
	bool UsesGraph(const UAnimNextGraph* InGraph) const;

	// Returns whether or not this graph instance is the root graph instance or false otherwise
	bool IsRoot() const;

	// Adds strong/hard object references during GC
	void AddStructReferencedObjects(class FReferenceCollector& Collector);

	// Returns a typed graph instance component, creating it lazily the first time it is queried
	template<class ComponentType>
	ComponentType& GetComponent();

	// Returns a typed graph instance component pointer if found or nullptr otherwise
	template<class ComponentType>
	ComponentType* TryGetComponent();

	// Returns a typed graph instance component pointer if found or nullptr otherwise
	template<class ComponentType>
	const ComponentType* TryGetComponent() const;

	// Returns const iterators to the graph instance component container
	GraphInstanceComponentMapType::TConstIterator GetComponentIterator() const;

private:
	// Returns a pointer to the specified component, or nullptr if not found
	UE::AnimNext::FGraphInstanceComponent* TryGetComponent(int32 ComponentNameHash, FName ComponentName) const;

	// Adds the specified component and returns a reference to it
	UE::AnimNext::FGraphInstanceComponent& AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>&& Component);

	// Indirection to hide implementation details and to fix the graph instance into a single memory location
	TUniquePtr<FAnimNextGraphInstance> Impl;

	friend UAnimNextGraph;					// The graph is the one that allocates instances
	friend FRigUnit_AnimNextGraphEvaluator;	// We evaluate the instance
	friend UE::AnimNext::FExecutionContext;
};

template<>
struct TStructOpsTypeTraits<FAnimNextGraphInstancePtr> : public TStructOpsTypeTraitsBase2<FAnimNextGraphInstancePtr>
{
	enum
	{
		WithAddStructReferencedObjects = true,
		WithCopy = false,
	};
};

//////////////////////////////////////////////////////////////////////////

template<class ComponentType>
ComponentType& FAnimNextGraphInstancePtr::GetComponent()
{
	const FName ComponentName = ComponentType::StaticComponentName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	if (UE::AnimNext::FGraphInstanceComponent* Component = TryGetComponent(ComponentNameHash, ComponentName))
	{
		return *static_cast<ComponentType*>(Component);
	}

	return static_cast<ComponentType&>(AddComponent(ComponentNameHash, ComponentName, MakeShared<ComponentType>()));
}

template<class ComponentType>
ComponentType* FAnimNextGraphInstancePtr::TryGetComponent()
{
	const FName ComponentName = ComponentType::StaticComponentName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
}

template<class ComponentType>
const ComponentType* FAnimNextGraphInstancePtr::TryGetComponent() const
{
	const FName ComponentName = ComponentType::StaticComponentName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
}
