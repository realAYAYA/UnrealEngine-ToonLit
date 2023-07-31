// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

namespace UE::Net
{

IRISCORE_API bool ShouldUseIrisReplication();
IRISCORE_API void SetUseIrisReplication(bool EnableIrisReplication);

}

/** Allow multiple replication systems inside the same process. Ex: PIE support */
#ifndef UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
#if (WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS || WITH_AUTOMATION_WORKER)
#	define UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS 1
#else
#	define UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS 0
#endif
#endif

/* NetBitStreamReader/Writer validation support */
#ifndef UE_NETBITSTREAMWRITER_VALIDATE
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define UE_NETBITSTREAMWRITER_VALIDATE 1
#else
#define UE_NETBITSTREAMWRITER_VALIDATE 0
#endif
#endif

/** CSV stats. Please check if CSV_PROFILER is enabled too if executing non-trivial code for stats. */
#ifndef UE_NET_IRIS_CSV_STATS
#	define UE_NET_IRIS_CSV_STATS 1
#endif

/** Verbose CSV stats is not recommended in shipping builds due to the expected CPU overhead. */
#if UE_NET_IRIS_CSV_STATS
#	ifndef UE_NET_IRIS_VERBOSE_CSV_STATS
#		define UE_NET_IRIS_VERBOSE_CSV_STATS !UE_BUILD_SHIPPING
#	endif
#else
// Force disable UE_NET_IRIS_VERBOSE_CSV_STATS if UE_NET_IRIS_CSV_STATS isn't enabled.
#	ifdef UE_NET_IRIS_VERBOSE_CSV_STATS
#		undef UE_NET_IRIS_VERBOSE_CSV_STATS
#	endif
#	define UE_NET_IRIS_VERBOSE_CSV_STATS 0
#endif

