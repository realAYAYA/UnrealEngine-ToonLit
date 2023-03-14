// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"


// This will use unreal's malloc instead of system malloc for mutable memory operations
#define USE_UNREAL_ALLOC_IN_MUTABLE				1

// This will enable the internal memory manager for mutable operations, reducing the number of operations
// but increasing the memory usage slightly. This only affects packaged builds, and it is not used in editor.
#define USE_INTERNAL_MEMORY_MANAGER_IN_MUTABLE	1

// This slows down mutable, but gathers some data about memory usage for debug
#define USE_STAT_MUTABLE_MEMORY					0


class MUTABLERUNTIME_API FMutableRuntimeModule : public IModuleInterface
{
public:

	// IModuleInterface 
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};
