// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Trace.h"

namespace UE::Trace { class FChannel; }

UE_TRACE_CHANNEL_EXTERN(MutableChannel, MUTABLERUNTIME_API)

/** Custom Mutable profiler scope. */
#define MUTABLE_CPUPROFILER_SCOPE(Name) \
	TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(Mutable_##Name, MutableChannel)
