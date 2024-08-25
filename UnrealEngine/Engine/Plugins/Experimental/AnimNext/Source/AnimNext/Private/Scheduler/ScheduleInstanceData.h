// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Scheduler/ScheduleHandle.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphInstancePtr.h"
#include "Param/ParamStack.h"

class UAnimNextSchedule;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FParamStack;
	struct FParamStackLayerHandle;
	class IParameterSource;
	struct FPropertyBagProxy;
}

namespace UE::AnimNext
{

// Host for all data needed to run a schedule instance
struct FScheduleInstanceData : public FGCObject
{
	FScheduleInstanceData(const FScheduleContext& InScheduleContext, const UAnimNextSchedule* InSchedule, FScheduleHandle InHandle, FAnimNextSchedulerEntry* InCurrentEntry);

	~FScheduleInstanceData();
	
	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	// Get the appropriate params stack given the ID
	TSharedPtr<FParamStack> GetParamStack(uint32 InIndex) const;
	
	// Handle to the currently executing entry in the schedule
	FScheduleHandle Handle;

	// Schedule entry that owns this instance
	FAnimNextSchedulerEntry* Entry = nullptr;

	// Scope for user parameters to be applied at the root of the schedule
	TUniquePtr<FPropertyBagProxy> RootUserScope;

	// Pushed layer for the root scope
	FParamStack::FPushedLayerHandle PushedRootUserLayer;

	struct FUserScope
	{
		// Layer that will be pushed before the scope, allowing the static scope to override the layer
		TUniquePtr<FPropertyBagProxy> BeforeSource;

		// Layer that will be pushed after the scope, overriding the static scope
		TUniquePtr<FPropertyBagProxy> AfterSource;
	};
	
	// Set of dynamic parameter scopes supplied by the user
	TMap<FName, FUserScope> UserScopes;

	// Cached data for each parameter scope
	struct FScopeCache
	{
		// Parameter sources at this scope
		TArray<TUniquePtr<IParameterSource>> ParameterSources;

		// Pushed layers, popped when scope exits
		TArray<FParamStack::FPushedLayerHandle> PushedLayers;
	};

	// Cached data for all param scopes
	TArray<FScopeCache> ScopeCaches;

	// Root param stack for the schedule itself (and globals)
	TSharedPtr<FParamStack> RootParamStack;

	// Param stacks required to run the schedule (one per task that requires a stack)
	TArray<TSharedPtr<FParamStack>> ParamStacks;

	// Intermediate data area
	FInstancedPropertyBag IntermediatesData;

	// Layer for intermediates data
	FParamStackLayerHandle IntermediatesLayer;

	// Remapped data layers for each port
	TArray<FParamStackLayerHandle> PortTermLayers;

	// Cached data for each graph
	struct FGraphCache
	{
		// Graph instance data
		FAnimNextGraphInstancePtr GraphInstanceData;

		// Remapped data layers for input terms (from schedule)
		FParamStackLayerHandle GraphTermLayer;
	};

	TArray<FGraphCache> GraphCaches;

	// Cached data for each external param task
	struct FExternalParamCache
	{
		// Parameter sources at this scope
		TArray<TUniquePtr<IParameterSource>> ParameterSources;
	};

	TArray<FExternalParamCache> ExternalParamCaches;
};

}