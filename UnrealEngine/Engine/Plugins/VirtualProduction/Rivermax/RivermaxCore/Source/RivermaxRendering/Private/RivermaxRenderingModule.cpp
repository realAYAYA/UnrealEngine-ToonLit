// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxRenderingModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"


void FRivermaxRenderingModule::StartupModule()
{
	const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("RivermaxCore"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/RivermaxCore"), PluginShaderDir);

}
IMPLEMENT_MODULE(FRivermaxRenderingModule, RivermaxRendering);
