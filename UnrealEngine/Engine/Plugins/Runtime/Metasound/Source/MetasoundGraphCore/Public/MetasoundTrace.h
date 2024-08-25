// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/LowLevelMemTracker.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#if !defined(METASOUND_CPUPROFILERTRACE_ENABLED)
#if CPUPROFILERTRACE_ENABLED
#define METASOUND_CPUPROFILERTRACE_ENABLED 1
#else
#define METASOUND_CPUPROFILERTRACE_ENABLED 0
#endif
#endif

#if METASOUND_CPUPROFILERTRACE_ENABLED
// Metasound CPU profiler trace enabled

// Copied from SCOPED_NAMED_EVENT but modified
// to accommodate event names containing ::
#define METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Name)\
	FScopedNamedEventStatic PREPROCESSOR_JOIN(MetaSound_NamedEvent_,__LINE__)(FColor::Green, NAMED_EVENT_STR(#Name));\
	TRACE_CPUPROFILER_EVENT_SCOPE(Name);

#define METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(Name) \
    SCOPED_NAMED_EVENT_TCHAR(Name, FColor::Green)
#else
// Metasound CPU profiler trace *not* enabled

#define METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Name)
#define METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(Name)

#endif

LLM_DECLARE_TAG_API(Audio_MetaSound, METASOUNDGRAPHCORE_API);
// Convenience macro for Audio_MetaSound LLM scope to avoid misspells.
#define METASOUND_LLM_SCOPE LLM_SCOPE_BYTAG(Audio_MetaSound);
