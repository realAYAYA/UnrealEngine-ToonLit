// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

namespace Metasound
{
	struct FNodeClassMetadata;

	// Debug util for setting stack scope debug variables
	namespace ThreadLocalDebug
	{
		// Sets the current active node class for the current thread.
		void SetActiveNodeClass(const FNodeClassMetadata& InMetadata);

		// Resets the current active node class for the current thread.
		void ResetActiveNodeClass();

		// Returns the class name and version string for the active node in the current thread.
		const TCHAR* GetActiveNodeClassNameAndVersion();
	}
}
