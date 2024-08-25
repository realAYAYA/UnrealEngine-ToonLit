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

class FPlatformEventsTrace
{
public:
	enum class EEventType
	{
		None = 0x00,
		ContextSwitch = 0x01,
		StackSampling = 0x02,
	};

	CORE_API static EEventType GetEvent(const FString& Name);

#if PLATFORM_SUPPORTS_PLATFORM_EVENTS

	CORE_API static void Init(uint32 SamplingIntervalUsec);
	CORE_API static void PostInit();
	CORE_API static void Enable(EEventType Event);
	CORE_API static void Disable(EEventType Event);
	CORE_API static void Stop();
	CORE_API static void OnTraceChannelUpdated(const FString& ChannelName, bool bIsEnabled);

	CORE_API static void OutputContextSwitch(uint64 StartTime, uint64 EndTime, uint32 ThreadId, uint8 CoreNumber);
	CORE_API static void OutputStackSample(uint64 Time, uint32 ThreadId, const uint64* Addresses, uint32 AddressCount);
	CORE_API static void OutputThreadName(uint32 ThreadId, uint32 ProcessId, const TCHAR* Name, uint32 NameLen);

#else // PLATFORM_SUPPORTS_PLATFORM_EVENTS

	CORE_API static void Init(uint32 SamplingIntervalUsec) {}
	CORE_API static void PostInit() {}
	CORE_API static void Enable(EEventType Event) {}
	CORE_API static void Disable(EEventType Event) {}
	CORE_API static void Stop() {}
	CORE_API static void OnTraceChannelUpdated(const FString& ChannelName, bool bIsEnabled) {}

	CORE_API static void OutputContextSwitch(uint64 StartTime, uint64 EndTime, uint32 ThreadId, uint8 CoreNumber) {}
	CORE_API static void OutputStackSample(uint64 Time, uint32 ThreadId, const uint64* Addresses, uint32 AddressCount) {}
	CORE_API static void OutputThreadName(uint32 ThreadId, uint32 ProcessId, const TCHAR* Name, uint32 NameLen) {}

#endif // PLATFORM_SUPPORTS_PLATFORM_EVENTS
};

ENUM_CLASS_FLAGS(FPlatformEventsTrace::EEventType)


/////////////////////////////////////////////////////////////////////
