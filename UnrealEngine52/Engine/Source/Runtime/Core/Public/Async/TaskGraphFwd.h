// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TaskGraphFwd.h: TaskGraph library
=============================================================================*/

#pragma once

#include "Templates/RefCounting.h"

#if !defined(TASKGRAPH_NEW_FRONTEND)
#define TASKGRAPH_NEW_FRONTEND 0
#endif

class FBaseGraphTask;

#if TASKGRAPH_NEW_FRONTEND

using FGraphEvent = FBaseGraphTask;
using FGraphEventRef = TRefCountPtr<FBaseGraphTask>;

#else

/** Convenience typedef for a reference counted pointer to a graph event **/
using FGraphEventRef = TRefCountPtr<class FGraphEvent>;

#endif
