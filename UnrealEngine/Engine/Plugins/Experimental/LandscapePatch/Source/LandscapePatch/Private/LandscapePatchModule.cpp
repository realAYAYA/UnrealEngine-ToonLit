// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapePatchModule.h"

#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h" // AddShaderSourceDirectoryMapping

#define LOCTEXT_NAMESPACE "FLandscapePatchModule"

void FLandscapePatchModule::StartupModule()
{
	// Maps virtual shader source directory to the plugin's actual Shaders directory.
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("LandscapePatch"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/LandscapePatch"), PluginShaderDir);
}

void FLandscapePatchModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLandscapePatchModule, LandscapePatch)