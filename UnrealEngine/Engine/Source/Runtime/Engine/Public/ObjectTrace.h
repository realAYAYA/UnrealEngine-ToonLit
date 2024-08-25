// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Trace/Config.h"
#include "Subsystems/WorldSubsystem.h"
#include "TraceFilter.h"

#include "ObjectTrace.generated.h"

#if UE_TRACE_ENABLED && !IS_PROGRAM && !UE_BUILD_SHIPPING
#define OBJECT_TRACE_ENABLED 1
#else
#define OBJECT_TRACE_ENABLED 0
#endif

// World subsystem used to track world info
UCLASS()
class UObjectTraceWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	UObjectTraceWorldSubsystem()
		: FrameIndex(0), RecordingIndex(0), ElapsedTime(0.0) 
	{}

public:
	// The frame index incremented each tick
	uint16 FrameIndex;
	// Trace Recording identifier  (set by RewindDebugger or 0)
	uint16 RecordingIndex;
	// Elapsed time since recording started  (or since start of game if RewindDebugger didn't start the trace)
	double ElapsedTime;
};

#if OBJECT_TRACE_ENABLED

class UClass;
class UObject;
class UWorld;
class FSceneView;

struct FObjectTrace
{
	/** Initialize object tracing */
	ENGINE_API static void Init();

	/** Shut down object tracing */
	ENGINE_API static void Destroy();
	
	/** Reset Caches so a new trace can be started*/
	ENGINE_API static void Reset();

	/** Helper function to output an object */
	ENGINE_API static void OutputClass(const UClass* InClass);

	/** Helper function to output an object */
	ENGINE_API static void OutputObject(const UObject* InObject);
	
	/** Helper function to output object create event */
	ENGINE_API static void OutputObjectLifetimeBegin(const UObject* InObject);
	
	/** Helper function to output object create event */
	ENGINE_API static void OutputObjectLifetimeEnd(const UObject* InObject);

	/** Helper function to output camera information for a player */
	ENGINE_API static void OutputView(const UObject* LocalPlayer, const FSceneView* View);

	/** Helper function to output an object event */
	ENGINE_API static void OutputObjectEvent(const UObject* InObject, const TCHAR* InEvent);
	
	/** Helper function to output controller attach event */
	ENGINE_API static void OutputPawnPossess(const UObject* InController, const UObject* InPawn);

	/** Helper function to get an object ID from a UObject */
	ENGINE_API static uint64 GetObjectId(const UObject* InObject);

	/** Helper function to get the UObject from an ObjectId, if the UObject still exists */
	ENGINE_API static UObject* GetObjectFromId(uint64 id);

	/** Helper function to get an object's world's tick counter */
	ENGINE_API static uint16 GetObjectWorldTickCounter(const UObject* InObject);

	/** reset the world elapsed time to 0 */
	ENGINE_API static void ResetWorldElapsedTime(const UWorld* InWorld);

	/** Helper function to get a world's elapsed time */
	ENGINE_API static double GetWorldElapsedTime(const UWorld* InWorld);

	/** Helper function to get an object's world's elapsed time */
	ENGINE_API static double GetObjectWorldElapsedTime(const UObject* InObject);

	/** Helper function to set a world's recording index */
	ENGINE_API static void SetWorldRecordingIndex(const UWorld *World, uint16 Index);

	/** Helper function to get a world's recording index */
	ENGINE_API static uint16 GetWorldRecordingIndex(const UWorld *InWorld);

	/** Helper function to get an object's world's recording index */
	ENGINE_API static uint16 GetObjectWorldRecordingIndex(const UObject* InObject);

	/** Helper function to output a world */
	ENGINE_API static void OutputWorld(const UWorld* InWorld);

};

#define TRACE_CLASS(Class) \
	FObjectTrace::OutputClass(Class);

#define TRACE_OBJECT(Object) \
	FObjectTrace::OutputObject(Object);

#define TRACE_VIEW(Player, View) \
	FObjectTrace::OutputView(Player, View);

#if TRACE_FILTERING_ENABLED

#define TRACE_OBJECT_EVENT(Object, Event) \
	if(CAN_TRACE_OBJECT(Object)) { UNCONDITIONAL_TRACE_OBJECT_EVENT(Object, Event); }
	
#define TRACE_OBJECT_LIFETIME_BEGIN(Object) \
	if(CAN_TRACE_OBJECT(Object)) { FObjectTrace::OutputObjectLifetimeBegin(Object); }

#define TRACE_OBJECT_LIFETIME_END(Object) \
	if(CAN_TRACE_OBJECT(Object)) { FObjectTrace::OutputObjectLifetimeEnd(Object); }

#define TRACE_PAWN_POSSESS(Controller, Pawn)\
	if(CAN_TRACE_OBJECT(Controller) && (Pawn==nullptr || (CAN_TRACE_OBJECT(Pawn)))) FObjectTrace::OutputPawnPossess(Controller, Pawn);

#else

#define TRACE_OBJECT_EVENT(Object, Event) \
	UNCONDITIONAL_TRACE_OBJECT_EVENT(Object, Event);

#define TRACE_OBJECT_LIFETIME_BEGIN(Object) \
	FObjectTrace::OutputObjectLifetimeBegin(Object);
	
#define TRACE_OBJECT_LIFETIME_END(Object) \
	FObjectTrace::OutputObjectLifetimeEnd(Object);
	
#define TRACE_PAWN_POSSESS(Controller, Pawn)\
	FObjectTrace::OutputPawnPossess(Controller, Pawn);

#endif
	
#define UNCONDITIONAL_TRACE_OBJECT_EVENT(Object, Event) \
	FObjectTrace::OutputObjectEvent(Object, TEXT(#Event));

#define TRACE_WORLD(World) \
	FObjectTrace::OutputWorld(World);


#else

struct FObjectTrace
{
	/** Helper function to get the UObject from an ObjectId, if the UObject still exists */
	ENGINE_API static UObject* GetObjectFromId(uint64 id) { return nullptr; }
};

#define TRACE_CLASS(Class)
#define TRACE_OBJECT(Object)
#define TRACE_OBJECT_EVENT(Object, Event)
#define TRACE_WORLD(World)
#define TRACE_PAWN_POSSESS(Controller, Pawn)
#define TRACE_VIEW(Player, View)
#define TRACE_OBJECT_LIFETIME_BEGIN(Object)
#define TRACE_OBJECT_LIFETIME_END(Object)

#endif