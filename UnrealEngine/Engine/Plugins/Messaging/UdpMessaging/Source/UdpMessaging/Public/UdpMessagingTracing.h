// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#if UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
UE_TRACE_CHANNEL_EXTERN(MessagingChannel, UDPMESSAGING_API);
#define SCOPED_MESSAGING_TRACE(TraceName) TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(TraceName, MessagingChannel)
#else
#define SCOPED_MESSAGING_TRACE(TraceName)
#endif
