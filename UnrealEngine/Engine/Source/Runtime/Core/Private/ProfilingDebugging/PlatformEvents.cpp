// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/PlatformEvents.h"
#include "Containers/UnrealString.h"

/////////////////////////////////////////////////////////////////////

UE_TRACE_CHANNEL_DEFINE(ContextSwitchChannel)
UE_TRACE_CHANNEL_DEFINE(StackSamplingChannel)

UE_TRACE_EVENT_DEFINE(PlatformEvent, ContextSwitch)
UE_TRACE_EVENT_DEFINE(PlatformEvent, StackSample)
UE_TRACE_EVENT_DEFINE(PlatformEvent, ThreadName)

/////////////////////////////////////////////////////////////////////

EPlatformEvent PlatformEvents_GetEvent(const FString& Name)
{
	if (Name == TEXT("contextswitch"))
	{
		return EPlatformEvent::ContextSwitch;
	}
	else if (Name == TEXT("stacksampling"))
	{
		return EPlatformEvent::StackSampling;
	}
	else
	{
		return EPlatformEvent::None;
	}
}

/////////////////////////////////////////////////////////////////////

#if PLATFORM_SUPPORTS_PLATFORM_EVENTS

void PlatformEvents_PostInit()
{
	if (TRACE_PRIVATE_CHANNELEXPR_IS_ENABLED(ContextSwitchChannel))
	{
		PlatformEvents_Enable(EPlatformEvent::ContextSwitch);
	}
	else if (TRACE_PRIVATE_CHANNELEXPR_IS_ENABLED(StackSamplingChannel))
	{
		PlatformEvents_Enable(EPlatformEvent::StackSampling);
	}
}

#endif // PLATFORM_SUPPORTS_PLATFORM_EVENTS

/////////////////////////////////////////////////////////////////////
