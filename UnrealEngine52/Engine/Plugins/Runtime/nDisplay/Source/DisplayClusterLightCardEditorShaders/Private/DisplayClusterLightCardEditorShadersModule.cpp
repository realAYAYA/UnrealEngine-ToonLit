// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterLightCardEditorShadersModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define NDISPLAY_SHADERS_MAP TEXT("/Plugin/nDisplay")

void FDisplayClusterLightCardEditorShadersModule::StartupModule()
{
	if (!AllShaderSourceDirectoryMappings().Contains(NDISPLAY_SHADERS_MAP))
	{
		FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("nDisplay"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(NDISPLAY_SHADERS_MAP, PluginShaderDir);
	}
}

void FDisplayClusterLightCardEditorShadersModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDisplayClusterLightCardEditorShadersModule, DisplayClusterLightCardEditorShaders);
