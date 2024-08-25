// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#ifndef IRIS_PROFILER_ENABLE
#	if (UE_BUILD_SHIPPING)
#		define IRIS_PROFILER_ENABLE 0
#	else
#		define IRIS_PROFILER_ENABLE 1
#	endif
#endif

// When true this adds dynamic protocol names in profile captures. The downside is a noticeable cpu cost overhead but only while cpu trace recording is occurring.
#ifndef UE_IRIS_PROFILER_ENABLE_PROTOCOL_NAMES
#	define UE_IRIS_PROFILER_ENABLE_PROTOCOL_NAMES !UE_BUILD_SHIPPING
#endif

// When true this adds low-level cpu trace captures of operations in Iris. Adds a little cpu overhead but only while cpu trace recording is occurring.
#ifndef UE_IRIS_PROFILER_ENABLE_VERBOSE
#	if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#		define UE_IRIS_PROFILER_ENABLE_VERBOSE 0
#	else
#		define UE_IRIS_PROFILER_ENABLE_VERBOSE 1
#	endif
#endif


//#define IRIS_USE_SUPERLUMINAL

#if IRIS_PROFILER_ENABLE
#	ifdef IRIS_USE_SUPERLUMINAL
#		include "c:/Program Files/Superluminal/Performance/API/include/Superluminal/PerformanceAPI.h"
#		include "HAL/PreprocessorHelpers.h"
#		pragma comment (lib, "c:/Program Files/Superluminal/Performance/API/lib/x64/PerformanceAPI_MD.lib")
#		define IRIS_PROFILER_SCOPE(x) PERFORMANCEAPI_INSTRUMENT(PREPROCESSOR_TO_STRING(x))
#		define IRIS_PROFILER_SCOPE_TEXT(x) PERFORMANCEAPI_INSTRUMENT_DATA(PREPROCESSOR_JOIN(IrisProfilerScope, __LINE__), x)
#	else
#		include "ProfilingDebugging/CpuProfilerTrace.h"
#		define IRIS_PROFILER_SCOPE(x) TRACE_CPUPROFILER_EVENT_SCOPE(x)
#		define IRIS_PROFILER_SCOPE_TEXT(X) TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(X)
#	endif
#else
#	define PERFORMANCEAPI_ENABLED 0
#	define IRIS_PROFILER_SCOPE(x)
#	define IRIS_PROFILER_SCOPE_TEXT(X)
#endif

#if UE_IRIS_PROFILER_ENABLE_PROTOCOL_NAMES
#	define IRIS_PROFILER_PROTOCOL_NAME(x) IRIS_PROFILER_SCOPE_TEXT(x)
#else
#	define IRIS_PROFILER_PROTOCOL_NAME(x)
#endif

#if UE_IRIS_PROFILER_ENABLE_VERBOSE
#	define IRIS_PROFILER_SCOPE_VERBOSE(x) IRIS_PROFILER_SCOPE(x);
#else
#	define IRIS_PROFILER_SCOPE_VERBOSE(x)
#endif
