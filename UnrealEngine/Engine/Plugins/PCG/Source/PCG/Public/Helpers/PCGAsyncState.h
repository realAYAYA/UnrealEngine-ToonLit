// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

/**
* Helper class to gather information about asynchronus execution. Will be held in the PCGContext.
*/
struct FPCGAsyncState
{
	/** For timeslicing, it will be set by the graph executor to know when we should stop. */
	double EndTime = 0.0;

	/** How many tasks are available to run async. */
	int32 NumAvailableTasks = 0;

	/** For async process, keep track of where the processing is, in the input data(read) and output data(write). */
	int32 AsyncCurrentReadIndex = 0;
	int32 AsyncCurrentWriteIndex = 0;

	/** For multithreading, track if the current element is run on the main thread.*/
	bool bIsRunningOnMainThread = true;

	/** True if currently inside a PCGAsync scope - will prevent further async processing */
	bool bIsRunningAsyncCall = false;

	/** Returns true if we reached end time. */
	PCG_API bool ShouldStop() const;
};