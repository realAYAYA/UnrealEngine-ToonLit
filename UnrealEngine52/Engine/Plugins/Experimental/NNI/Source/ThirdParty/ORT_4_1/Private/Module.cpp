// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module.h"

#include "Modules/ModuleManager.h"

// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
void FONNXRuntimeModule::StartupModule()
{
}

// This function may be called during shutdown to clean up your module. For modules that support dynamic reloading,
// we call this function before unloading the module.
void FONNXRuntimeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FONNXRuntimeModule, ONNXRuntime);
