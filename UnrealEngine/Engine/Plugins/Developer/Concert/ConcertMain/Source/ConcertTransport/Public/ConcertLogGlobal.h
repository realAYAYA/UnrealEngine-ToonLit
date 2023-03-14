// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#if UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
UE_TRACE_CHANNEL_EXTERN(ConcertChannel, CONCERTTRANSPORT_API);
#define SCOPED_CONCERT_TRACE(TraceName) TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(TraceName, ConcertChannel)
#else
#define SCOPED_CONCERT_TRACE(TraceName)
#endif

// The default debug verbosity level can be set as a GlobalDefinitions in an executable Target.cs build file to adapt to this executable context.
#ifndef UE_LOG_CONCERT_DEBUG_VERBOSITY_LEVEL
#define UE_LOG_CONCERT_DEBUG_VERBOSITY_LEVEL Error
#endif

CONCERTTRANSPORT_API DECLARE_LOG_CATEGORY_EXTERN(LogConcert, Log, All);
CONCERTTRANSPORT_API DECLARE_LOG_CATEGORY_EXTERN(LogConcertDebug, UE_LOG_CONCERT_DEBUG_VERBOSITY_LEVEL, All);
