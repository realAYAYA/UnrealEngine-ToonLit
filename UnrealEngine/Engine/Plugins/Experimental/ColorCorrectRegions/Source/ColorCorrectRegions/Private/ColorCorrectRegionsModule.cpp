// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorCorrectRegionsModule.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Runtime/RenderCore/Public/ShaderCore.h"


#define LOCTEXT_NAMESPACE "FColorCorrectRegionsModule"

void FColorCorrectRegionsModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ColorCorrectRegions"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/ColorCorrectRegionsShaders"), PluginShaderDir);
}

void FColorCorrectRegionsModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FColorCorrectRegionsModule, ColorCorrectRegions);
DEFINE_LOG_CATEGORY(ColorCorrectRegions);
