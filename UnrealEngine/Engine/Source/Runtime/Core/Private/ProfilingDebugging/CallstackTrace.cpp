// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/CallstackTrace.h"
#include "CallstackTracePrivate.h"

#if UE_CALLSTACK_TRACE_ENABLED

// Platform implementations of back tracing
////////////////////////////////////////////////////////////////////////////////
void	CallstackTrace_CreateInternal(FMalloc*);
void	CallstackTrace_InitializeInternal();

////////////////////////////////////////////////////////////////////////////////
UE_TRACE_CHANNEL_DEFINE(CallstackChannel)
UE_TRACE_EVENT_DEFINE(Memory, CallstackSpec)

uint32 GCallStackTracingTlsSlotIndex = FPlatformTLS::InvalidTlsSlot;

////////////////////////////////////////////////////////////////////////////////
void CallstackTrace_Create(class FMalloc* InMalloc)
{
	static auto InitOnce = [&]
	{
		CallstackTrace_CreateInternal(InMalloc);
		return true;
	}();
}

////////////////////////////////////////////////////////////////////////////////
void CallstackTrace_Initialize()
{
	GCallStackTracingTlsSlotIndex = FPlatformTLS::AllocTlsSlot();
	//NOTE: we don't bother cleaning up TLS, this is only closed during real shutdown

	static auto InitOnce = [&]
	{
		CallstackTrace_InitializeInternal();
		return true;
	}();
}

#endif //UE_CALLSTACK_TRACE_ENABLED