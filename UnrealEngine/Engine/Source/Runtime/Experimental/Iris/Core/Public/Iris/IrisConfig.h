// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#include "Net/Core/Connection/NetEnums.h"
#include "ProfilingDebugging/CsvProfilerConfig.h"

namespace UE::Net
{

/** Returns if the preferred replication system should be Iris. */
IRISCORE_API bool ShouldUseIrisReplication();

/** Set if the preferred replication system should be Iris or not. */
IRISCORE_API void SetUseIrisReplication(bool EnableIrisReplication);

/** Returns what replication sytem was set to be used by the cmdline. Returns Default when the command line was not set. */
IRISCORE_API EReplicationSystem GetUseIrisReplicationCmdlineValue();

}

/* NetBitStreamReader/Writer validation support */
#ifndef UE_NETBITSTREAMWRITER_VALIDATE
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define UE_NETBITSTREAMWRITER_VALIDATE 1
#else
#define UE_NETBITSTREAMWRITER_VALIDATE 0
#endif
#endif

/** CSV stats. */
#ifndef UE_NET_IRIS_CSV_STATS
#	define UE_NET_IRIS_CSV_STATS CSV_PROFILER
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

/** Enables code that detects non-thread safe access to network data */
#ifndef UE_NET_THREAD_SAFETY_CHECK
#	define UE_NET_THREAD_SAFETY_CHECK DO_CHECK
#endif