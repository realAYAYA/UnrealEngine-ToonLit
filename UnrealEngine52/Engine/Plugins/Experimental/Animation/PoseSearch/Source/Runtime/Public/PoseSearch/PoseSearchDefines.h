// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/ParallelFor.h"
#include "Logging/LogMacros.h"
#include "ObjectTrace.h"

// Enable this if object tracing is enabled, mimics animation tracing
#define UE_POSE_SEARCH_TRACE_ENABLED OBJECT_TRACE_ENABLED

#ifndef UE_POSE_SEARCH_FORCE_SINGLE_THREAD
#define UE_POSE_SEARCH_FORCE_SINGLE_THREAD 0
#endif

#if UE_POSE_SEARCH_FORCE_SINGLE_THREAD
constexpr EParallelForFlags ParallelForFlags = EParallelForFlags::ForceSingleThread;
#else
constexpr EParallelForFlags ParallelForFlags = EParallelForFlags::None;
#endif // UE_POSE_SEARCH_FORCE_SINGLE_THREAD

DECLARE_LOG_CATEGORY_EXTERN(LogPoseSearch, Log, All);
