// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Build.h"
#include "Trace/Config.h"

#if UE_TRACE_ENABLED && !IS_PROGRAM && !UE_BUILD_SHIPPING
#define SOURCE_FILTER_TRACE_ENABLED 1
#else
#define SOURCE_FILTER_TRACE_ENABLED 0
#endif

#if SOURCE_FILTER_TRACE_ENABLED

#include "CoreTypes.h"
#include "Trace/Trace.h"
#include "UObject/ObjectKey.h"
#include "Templates/SubclassOf.h"

#include "ObjectTrace.h"
#include "DataSourceFiltering.h"
#include "DataSourceFilter.h"
#include "DataSourceFilterSet.h"

UE_TRACE_CHANNEL_EXTERN(TraceSourceFiltersChannel)

struct SOURCEFILTERINGTRACE_API FSourceFilterTrace
{
	/** Output trace data for a UDataSourceFilter (sub) class */
	static void OutputClass(const TSubclassOf<UDataSourceFilter> InClass);
	/** Output trace data for a UDataSourceFilter object instance */
	static void OutputInstance(const UDataSourceFilter* InFilter);
	/** Output trace data for a UDataSourceFilterSet object instance*/
	static void OutputSet(const UDataSourceFilterSet* InFilterSet);
	/** Output trace data for an operation involving a UDataSourceFilter(Set) instance */
	static void OutputFilterOperation(const UDataSourceFilter* InFilter, ESourceActorFilterOperation Operation, uint64 Parameter);

	/** Output trace data for a change in the UTraceSourceFilteringSettings for this running instance */
	static void OutputFilterSettingsValue(const FString& InPropertyName, const uint8 InValue);

	/** Output trace data for a UWorld's filtering related information */
	static void OutputWorld(const UWorld* InWorld);
	/** Output trace data for an operation involving a UWorld instance */
	static void OutputWorldOperation(const UWorld* InWorld, EWorldFilterOperation Operation, uint32 Parameter);	

	/** Tries to retrieve a UClass instance according to its Object Identifier */
	static UClass* RetrieveClassById(uint64 ClassId);
	/** Tries to retrieve a UClass instance according to its name */
	static UClass* RetrieveClassByName(const FString& ClassName);

	/** Tries to retrieve a UDataSourceFilter instance according to its Object Identifier */
	static UDataSourceFilter* RetrieveFilterbyId(uint64 FilterId);
	/** Tries to retrieve a UWorld instance according to its Object Identifier */
	static UWorld* RetrieveWorldById(uint64 WorldId);

protected:
	/** Mapping from a UClass's ObjectKey to their Object identifier retrieved from FObjectTrace::GetObjectId */
	static TMap<FObjectKey, uint64> FilterClassIds;
	/** Mapping from a UClass's Object identifier to an FObjectKey */
	static TMap<uint64, FObjectKey> IDToFilterClass;

	/** Mapping from a UDataSourceFilter's Object identifier to their FObjectKey */
	static TMap<uint64, FObjectKey> IDToFilter;

	/** Set of FObjectKey's, representing UDataSourceFilter instances, which have previously been traced out */
	static TSet<FObjectKey> FilterInstances;

	/** Mapping from an UClass's (sub class of UDataSourceFilter) name to their FObjectKey */
	static TMap<FString, FObjectKey> DataSourceFilterClasses;
	
	/** Mapping from an Object identifier (generated from a UWorld instance) to its FObjectKey */
	static TMap<uint64, FObjectKey> IDsToWorldInstance;
};

#define TRACE_FILTER_CLASS(Class) \
	FSourceFilterTrace::OutputClass(Class);

#define TRACE_FILTER_INSTANCE(Instance) \
	FSourceFilterTrace::OutputInstance(Instance);

#define TRACE_FILTER_SET(Set) \
	FSourceFilterTrace::OutputSet(Set);

#define TRACE_FILTER_OPERATION(Instance, Operation, Parameter) \
	FSourceFilterTrace::OutputFilterOperation(Instance, Operation, Parameter);

#define TRACE_FILTER_SETTINGS_VALUE(Name, Value) \
	FSourceFilterTrace::OutputFilterSettingsValue(Name, Value);

#define TRACE_WORLD_INSTANCE(World) \
	FSourceFilterTrace::OutputWorld(World);

#define TRACE_WORLD_OPERATION(Instance, Operation, Parameter) \
	FSourceFilterTrace::OutputWorldOperation(Instance, Operation, Parameter);

#define TRACE_FILTER_IDENTIFIER(Object) \
	FObjectTrace::GetObjectId(Object)
#else

#define TRACE_FILTER_CLASS(Class)
#define TRACE_FILTER_INSTANCE(Filter)
#define TRACE_FILTER_SET(Set)
#define TRACE_FILTER_OPERATION(Instance, Operation, Parameter)
#define TRACE_FILTER_SETTINGS_VALUE(Name, Value)

#define TRACE_WORLD_INSTANCE(World)
#define TRACE_WORLD_OPERATION(Instance, Operation, Parameter)

#define TRACE_FILTER_IDENTIFIER(Object) 0

#endif