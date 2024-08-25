// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProfilingDebugging/CallstackTrace.h"

#if UE_CALLSTACK_TRACE_ENABLED

#include "CoreTypes.h"

uint32 CallstackTrace_GetExternalCallstackId(uint64* Frames, uint32 Count);

#endif