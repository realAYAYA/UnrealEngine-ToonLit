// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeTests.h"
#include "Modules/ModuleManager.h"
#include "ShaderComparisonTests.h"

#define LOCTEXT_NAMESPACE "FRuntimeTestsModule"

void FRuntimeTestsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FCompareBasepassShadersAutomationTestInstance = new FCompareBasepassShaders( TEXT("FCompareBasepassShaders") );
	check(FCompareBasepassShadersAutomationTestInstance != nullptr);
}

void FRuntimeTestsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	delete FCompareBasepassShadersAutomationTestInstance;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRuntimeTestsModule, RuntimeTests)
