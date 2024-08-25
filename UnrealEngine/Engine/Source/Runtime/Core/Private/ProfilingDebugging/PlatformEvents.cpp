// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/PlatformEvents.h"
#include "Containers/UnrealString.h"

/////////////////////////////////////////////////////////////////////

UE_TRACE_CHANNEL_DEFINE(ContextSwitchChannel)
UE_TRACE_CHANNEL_DEFINE(StackSamplingChannel)

/////////////////////////////////////////////////////////////////////

// represents time interval when thread was running on specific core
UE_TRACE_EVENT_BEGIN(PlatformEvent, ContextSwitch, NoSync)
UE_TRACE_EVENT_FIELD(uint64, StartTime)
UE_TRACE_EVENT_FIELD(uint64, EndTime)
UE_TRACE_EVENT_FIELD(uint32, ThreadId)
UE_TRACE_EVENT_FIELD(uint8, CoreNumber)
UE_TRACE_EVENT_END()

// represents call stack addresses in stack sampling
UE_TRACE_EVENT_BEGIN(PlatformEvent, StackSample, NoSync)
UE_TRACE_EVENT_FIELD(uint64, Time)
UE_TRACE_EVENT_FIELD(uint32, ThreadId)
UE_TRACE_EVENT_FIELD(uint64[], Addresses)
UE_TRACE_EVENT_END()

// thread information for context switches
// NOTE: in some cases name of process can be empty, for example when there are no
// privileges to query it or there are no name for process thread belongs to.
// Depending on platform actual name can be absolute path to process executable.
UE_TRACE_EVENT_BEGIN(PlatformEvent, ThreadName, NoSync)
UE_TRACE_EVENT_FIELD(uint32, ThreadId)
UE_TRACE_EVENT_FIELD(uint32, ProcessId)
UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
UE_TRACE_EVENT_END()

/////////////////////////////////////////////////////////////////////

#if PLATFORM_SUPPORTS_PLATFORM_EVENTS

void FPlatformEventsTrace::OutputContextSwitch(uint64 StartTime, uint64 EndTime, uint32 ThreadId, uint8 CoreNumber)
{
	UE_TRACE_LOG(PlatformEvent, ContextSwitch, ContextSwitchChannel)
		<< ContextSwitch.StartTime(StartTime)
		<< ContextSwitch.EndTime(EndTime)
		<< ContextSwitch.ThreadId(ThreadId)
		<< ContextSwitch.CoreNumber(CoreNumber);
}

/////////////////////////////////////////////////////////////////////

void FPlatformEventsTrace::OutputStackSample(uint64 Time, uint32 ThreadId, const uint64* Addresses, uint32 AddressCount)
{
	UE_TRACE_LOG(PlatformEvent, StackSample, StackSamplingChannel)
		<< StackSample.Time(Time)
		<< StackSample.ThreadId(ThreadId)
		<< StackSample.Addresses(Addresses, AddressCount);
}

/////////////////////////////////////////////////////////////////////

void FPlatformEventsTrace::OutputThreadName(uint32 ThreadId, uint32 ProcessId, const TCHAR* Name, uint32 NameLen)
{
	UE_TRACE_LOG(PlatformEvent, ThreadName, ContextSwitchChannel)
		<< ThreadName.ThreadId(ThreadId)
		<< ThreadName.ProcessId(ProcessId)
		<< ThreadName.Name(Name, NameLen);
}

/////////////////////////////////////////////////////////////////////

FPlatformEventsTrace::EEventType FPlatformEventsTrace::GetEvent(const FString& Name)
{
	if (Name == TEXT("contextswitch"))
	{
		return FPlatformEventsTrace::EEventType::ContextSwitch;
	}
	else if (Name == TEXT("stacksampling"))
	{
		return FPlatformEventsTrace::EEventType::StackSampling;
	}
	else
	{
		return FPlatformEventsTrace::EEventType::None;
	}
}

/////////////////////////////////////////////////////////////////////

void FPlatformEventsTrace::OnTraceChannelUpdated(const FString& ChannelName, bool bIsEnabled)
{
	FPlatformEventsTrace::EEventType Event = FPlatformEventsTrace::GetEvent(ChannelName.Replace(TEXT("Channel"), TEXT("")));

	if (Event != FPlatformEventsTrace::EEventType::None)
	{
		if (bIsEnabled)
		{
			FPlatformEventsTrace::Enable(Event);
		}
		else
		{
			FPlatformEventsTrace::Disable(Event);
		}
	}
}

/////////////////////////////////////////////////////////////////////

void FPlatformEventsTrace::PostInit()
{
	if (TRACE_PRIVATE_CHANNELEXPR_IS_ENABLED(ContextSwitchChannel)) //-V517
	{
		Enable(FPlatformEventsTrace::EEventType::ContextSwitch);
	}
	else if (TRACE_PRIVATE_CHANNELEXPR_IS_ENABLED(StackSamplingChannel))
	{
		Enable(FPlatformEventsTrace::EEventType::StackSampling);
	}
}

#endif // PLATFORM_SUPPORTS_PLATFORM_EVENTS

/////////////////////////////////////////////////////////////////////
