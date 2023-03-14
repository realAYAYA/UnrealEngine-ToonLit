// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProfilingDebugging/CpuProfilerTrace.h"

#if !defined(AUDIO_MIXER_CPUPROFILERTRACE_ENABLED)
#if CPUPROFILERTRACE_ENABLED
#define AUDIO_MIXER_CPUPROFILERTRACE_ENABLED 1
#else
#define AUDIO_MIXER_CPUPROFILERTRACE_ENABLED 0
#endif
#endif

#if AUDIO_MIXER_CPUPROFILERTRACE_ENABLED
// AudioMixer CPU profiler trace enabled

#define AUDIO_MIXER_TRACE_CPUPROFILER_EVENT_SCOPE(Name) \
    TRACE_CPUPROFILER_EVENT_SCOPE(Name)

#else
// AudioMixer CPU profiler trace *not* enabled

#define AUDIO_MIXER_TRACE_CPUPROFILER_EVENT_SCOPE(Name)

#endif

