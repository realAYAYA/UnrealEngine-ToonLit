// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace mu
{
	/** This should be called only before starting to use the library or any of its data. */
	MUTABLERUNTIME_API extern void Initialize();

    /** Shutdown everything related to mutable.
     * This should be called when no other mutable objects will be used, created or destroyed,
     * and all threads involving mutable tasks have been terminated.
	 */
	MUTABLERUNTIME_API extern void Finalize();

}

