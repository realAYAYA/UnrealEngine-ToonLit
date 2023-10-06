// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/EnumClassFlags.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"

class FString;

#if !defined(PLATFORM_SUPPORTS_PLATFORM_EVENTS)
#	define PLATFORM_SUPPORTS_PLATFORM_EVENTS 0
#endif

/////////////////////////////////////////////////////////////////////

UE_TRACE_CHANNEL_EXTERN(ContextSwitchChannel)
UE_TRACE_CHANNEL_EXTERN(StackSamplingChannel)

/////////////////////////////////////////////////////////////////////

// represents time interval when thread was running on specific core
UE_TRACE_EVENT_BEGIN_EXTERN(PlatformEvent, ContextSwitch, NoSync)
	UE_TRACE_EVENT_FIELD(uint64, StartTime)
	UE_TRACE_EVENT_FIELD(uint64, EndTime)
	UE_TRACE_EVENT_FIELD(uint32, ThreadId)
	UE_TRACE_EVENT_FIELD(uint8, CoreNumber)
UE_TRACE_EVENT_END()

// represents call stack addresses in stack sampling
UE_TRACE_EVENT_BEGIN_EXTERN(PlatformEvent, StackSample, NoSync)
	UE_TRACE_EVENT_FIELD(uint64, Time)
	UE_TRACE_EVENT_FIELD(uint32, ThreadId)
	UE_TRACE_EVENT_FIELD(uint64[], Addresses)
UE_TRACE_EVENT_END()

// thread information for context switches
// NOTE: in some cases name of process can be empty, for example when there are no
// privileges to query it or there are no name for process thread belongs to.
// Depending on platform actual name can be absolute path to process executable.
UE_TRACE_EVENT_BEGIN_EXTERN(PlatformEvent, ThreadName, NoSync)
	UE_TRACE_EVENT_FIELD(uint32, ThreadId)
	UE_TRACE_EVENT_FIELD(uint32, ProcessId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
UE_TRACE_EVENT_END()

/////////////////////////////////////////////////////////////////////

enum class EPlatformEvent
{
	None = 0x00,
	ContextSwitch = 0x01,
	StackSampling = 0x02,
};

ENUM_CLASS_FLAGS(EPlatformEvent)

/////////////////////////////////////////////////////////////////////

EPlatformEvent PlatformEvents_GetEvent(const FString& Name);

#if PLATFORM_SUPPORTS_PLATFORM_EVENTS

void PlatformEvents_Init(uint32 SamplingIntervalUsec);
void PlatformEvents_PostInit();
void PlatformEvents_Enable(EPlatformEvent Event);
void PlatformEvents_Disable(EPlatformEvent Event);
void PlatformEvents_Stop();

#else // PLATFORM_SUPPORTS_PLATFORM_EVENTS

static void PlatformEvents_Init(uint32 SamplingIntervalUsec) {}
static void PlatformEvents_PostInit() {}
static void PlatformEvents_Enable(EPlatformEvent Event) {}
static void PlatformEvents_Disable(EPlatformEvent Event) {}
static void PlatformEvents_Stop() {}

#endif // PLATFORM_SUPPORTS_PLATFORM_EVENTS

/////////////////////////////////////////////////////////////////////
