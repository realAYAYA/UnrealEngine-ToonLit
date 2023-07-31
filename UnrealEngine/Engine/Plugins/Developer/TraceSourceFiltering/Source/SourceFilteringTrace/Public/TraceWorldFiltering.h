// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Engine/World.h"

class FSourceFilterManager;

/** UWorld specific Trace filter, marks individual instances to not be traced out (and all containing actors / objects) */
struct SOURCEFILTERINGTRACE_API FTraceWorldFiltering
{
	DECLARE_MULTICAST_DELEGATE(FTraceWorldFilterStateChanged);

	static void Initialize();
	static void Destroy();
	
	/** Retrieve a FSourceFilterManager instance representing the source filtering for a specific, provided) World instance */
	static const FSourceFilterManager* GetWorldSourceFilterManager(const UWorld* World);

	/** Return all Worlds currently tracked for Filtering */
	static const TArray<const UWorld*>& GetWorlds();

	/** Check whether or not a specific World Type can output Trace Data (not filtered out) */
	static bool IsWorldTypeTraceable(EWorldType::Type InType);
	/** Check whether or not a specific World's Net Mode can output Trace Data (not filtered out) */
	static bool IsWorldNetModeTraceable(ENetMode InNetMode);
	
	/** Set whether or not a specific World Type should be filtered out (or in) */
	static void SetStateByWorldType(EWorldType::Type WorldType, bool bState);	
	/** Set whether or not a specific World Net Mode should be filtered out (or in) */
	static void SetStateByWorldNetMode(ENetMode NetMode, bool bState);
	/** Set whether or not a specific UWorld instance's filtering state */
	static void SetWorldState(const UWorld* InWorld, bool bState);

	/** Returns a user facing display string for the provided UWorld instance */
	static void GetWorldDisplayString(const UWorld* InWorld, FString& OutDisplayString);

	/** Delegate which will be broadcast whenever the filtering state for any world (type, netmode) changes */
	static FTraceWorldFilterStateChanged& OnFilterStateChanged();

protected:
	/** Callbacks used to keep tracking of active (alive) UWorld instances */
	static void OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
	static void OnWorldPostInit(UWorld* World, const UWorld::InitializationValues IVS);
	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	static void RemoveWorld(UWorld* InWorld);

	static void UpdateWorldFiltering();

protected:
	static FDelegateHandle WorldInitHandle;
	static FDelegateHandle WorldPostInitHandle;
	static FDelegateHandle WorldBeginTearDownHandle;
	static FDelegateHandle WorldCleanupHandle;
	static FDelegateHandle PreWorldFinishDestroyHandle;

	/** Delegate for broadcasting filtering changes */
	static FTraceWorldFilterStateChanged FilterStateChangedDelegate;

	/** Array of currently active and alive UWorlds */
	static TArray<const UWorld*> Worlds;
	/** Per EWorldType enum entry flag, determines whether or not UWorld's of this type should be filtered out */
	static TMap<EWorldType::Type, bool> WorldTypeFilterStates;
	/** Per ENetMode enum entry flag, determines whether or not UWorld's using this netmode should be filtered out */
	static TMap<ENetMode, bool> NetModeFilterStates;
	/** Synchronization object for accessing WorldTypeFilterStates and NetModeFilterStates, required to ensure there is not competing access between Networking and Gamethread */
	static FCriticalSection WorldFilterStatesCritical;
	/** Mapping from UWorld instance to FSourceFilterManager, entries correspond to world instances in Worlds */
	static TMap<const UWorld*, FSourceFilterManager*> WorldSourceFilterManagers;
};