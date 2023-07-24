// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef IRIS_PROFILER_ENABLE
#	if (UE_BUILD_SHIPPING)
#		define IRIS_PROFILER_ENABLE 0
#	else
#		define IRIS_PROFILER_ENABLE 1
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
