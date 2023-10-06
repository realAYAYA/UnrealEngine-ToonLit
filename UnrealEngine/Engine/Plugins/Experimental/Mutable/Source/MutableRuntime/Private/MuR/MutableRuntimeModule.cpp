// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/MutableRuntimeModule.h"

#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "MuR/MutableMemory.h"
#include "MuR/Platform.h"

IMPLEMENT_MODULE(FMutableRuntimeModule, MutableRuntime);

DEFINE_LOG_CATEGORY(LogMutableCore);


void FMutableRuntimeModule::StartupModule()
{
	mu::Initialize();
}


void FMutableRuntimeModule::ShutdownModule()
{
	// Finalize the mutable runtime in this module
	mu::Finalize();
}

