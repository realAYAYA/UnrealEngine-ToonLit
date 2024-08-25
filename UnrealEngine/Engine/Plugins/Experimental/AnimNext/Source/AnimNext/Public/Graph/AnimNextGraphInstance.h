// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "RigVMCore/RigVMExecuteContext.h"

#include "AnimNextGraphInstance.generated.h"

class FReferenceCollector;

struct FAnimNextGraphInstancePtr;
struct FRigUnit_AnimNextGraphEvaluator;
class UAnimNextGraph;

namespace UE::AnimNext
{
	struct FExecutionContext;
	struct FGraphInstanceComponent;
	struct FLatentPropertyHandle;
}

using GraphInstanceComponentMapType = TMap<FName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>>;

// Represents an instance of an AnimNext graph
// This struct uses UE reflection because we wish for the GC to keep the graph
// alive while we own a reference to it. It is not intended to be serialized on disk with a live instance.
USTRUCT()
struct FAnimNextGraphInstance
{
	GENERATED_BODY()

	// Creates an empty graph instance that doesn't reference anything
	FAnimNextGraphInstance() = default;

	// No copying, no moving
	FAnimNextGraphInstance(const FAnimNextGraphInstance&) = delete;
	FAnimNextGraphInstance& operator=(const FAnimNextGraphInstance&) = delete;

	// If the graph instance is allocated, we release it during destruction
	~FAnimNextGraphInstance();

	// Releases the graph instance and frees all corresponding memory
	void Release();

	// Returns true if we have a live graph instance, false otherwise
	bool IsValid() const;

	// Returns the graph used by this instance or nullptr if the instance is invalid
	const UAnimNextGraph* GetGraph() const;

	// Returns the entry point in Graph that this instance corresponds to 
	FName GetEntryPoint() const;
	
	// Returns a weak handle to the root decorator instance
	UE::AnimNext::FWeakDecoratorPtr GetGraphRootPtr() const;

	// Returns the parent graph instance that owns us or nullptr for the root graph instance or if we are invalid
	FAnimNextGraphInstance* GetParentGraphInstance() const;

	// Returns the root graph instance that owns us and the components or nullptr if we are invalid
	FAnimNextGraphInstance* GetRootGraphInstance() const;

	// Check to see if this instance data matches the provided graph
	bool UsesGraph(const UAnimNextGraph* InGraph) const;

	// Check to see if this instance data matches the provided graph entry point
	bool UsesEntryPoint(FName InEntryPoint) const;
	
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

	// Executes a list of latent RigVM pins and writes the result into the destination pointer (latent handle offsets are using the destination as base)
	// When frozen, latent handles that can freeze are skipped, all others will execute
	void ExecuteLatentPins(const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& LatentHandles, void* DestinationBasePtr, bool bIsFrozen);

	// During graph compilation, if we have existing graph instances, we freeze them by releasing their memory before thawing them
	// Freezing is a partial release of resources that retains the necessary information to re-create things safely
	void Freeze();

	// During graph compilation, once compilation is done we thaw existing graph instances to reallocate their memory
	void Thaw();

	// Hard reference to the graph used to create this instance to ensure we can release it safely
	UPROPERTY()
	TObjectPtr<const UAnimNextGraph> Graph;

	// The entry point in Graph that this instance corresponds to 
	FName EntryPoint;

	// Hard reference to the graph instance data, we own it
	UE::AnimNext::FDecoratorPtr GraphInstancePtr;

	// The graph instance that owns us
	FAnimNextGraphInstance* ParentGraphInstance = nullptr;

	// The root graph instance that owns us and the components
	FAnimNextGraphInstance* RootGraphInstance = nullptr;

	// Extended execute context instance for this graph instance, we own it
	UPROPERTY()
	FRigVMExtendedExecuteContext ExtendedExecuteContext;

	// Graph instance components that persist from update to update
	GraphInstanceComponentMapType Components;

	friend UAnimNextGraph;					// The graph is the one that allocates instances
	friend FRigUnit_AnimNextGraphEvaluator;	// We evaluate the instance
	friend UE::AnimNext::FExecutionContext;
	friend FAnimNextGraphInstancePtr;
};

template<>
struct TStructOpsTypeTraits<FAnimNextGraphInstance> : public TStructOpsTypeTraitsBase2<FAnimNextGraphInstance>
{
	enum
	{
		WithAddStructReferencedObjects = true,
		WithCopy = false,
	};
};

//////////////////////////////////////////////////////////////////////////

template<class ComponentType>
ComponentType& FAnimNextGraphInstance::GetComponent()
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
ComponentType* FAnimNextGraphInstance::TryGetComponent()
{
	const FName ComponentName = ComponentType::StaticComponentName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
}

template<class ComponentType>
const ComponentType* FAnimNextGraphInstance::TryGetComponent() const
{
	const FName ComponentName = ComponentType::StaticComponentName();
	const int32 ComponentNameHash = GetTypeHash(ComponentName);

	return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
}
