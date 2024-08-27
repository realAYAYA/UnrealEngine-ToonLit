// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomRenderingPass.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FCustomRenderingPassModule"

void FCustomRenderingPassModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	const FString VertexShaderPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("CustomRenderingPass"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/CustomRenderingPass"), VertexShaderPath);
}

void FCustomRenderingPassModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCustomRenderingPassModule, CustomRenderingPass)