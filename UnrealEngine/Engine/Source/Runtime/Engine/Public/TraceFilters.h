// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceFilter.h"

#if TRACE_FILTERING_ENABLED

/** AActor specific Trace filter, marks individual instances to not be traced out */
struct ENGINE_API FTraceActorFilter
{
	static void Initialize();
	static void Destroy();
protected:
	static FDelegateHandle WorldPreInitHandle;
	static FDelegateHandle WorldPostInitHandle;
	static FDelegateHandle OnActorSpawnedHandle;
	static void OnPreWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
	static void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
	static void OnActorSpawned(AActor* Actor);
};

/** UWorld specific Trace filter, marks individual instances to not be traced out */
struct ENGINE_API FTraceWorldFilter
{
	static void Initialize();
	static void Destroy();
protected:
	static void OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS);	
	static FDelegateHandle WorldInitHandle;	
};

#define DISABLE_ENGINE_ACTOR_TRACE_FILTERING() \
	FTraceActorFilter::Destroy()

#define DISABLE_ENGINE_WORLD_TRACE_FILTERING() \
	FTraceWorldFilter::Destroy()

#else

#define DISABLE_ENGINE_ACTOR_TRACE_FILTERING()	
#define DISABLE_ENGINE_WORLD_TRACE_FILTERING() 

#endif // TRACE_FILTERING_ENABLED
