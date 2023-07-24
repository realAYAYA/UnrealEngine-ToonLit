// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOModule.h"

#include "Interfaces/IPluginManager.h"
#include "OpenColorIOLibHandler.h"
#include "OpenColorIODisplayManager.h"
#include "ShaderCore.h"


DEFINE_LOG_CATEGORY(LogOpenColorIO);

#define LOCTEXT_NAMESPACE "OpenColorIOModule"

FOpenColorIOModule::FOpenColorIOModule()
	: DisplayManager(MakeUnique<FOpenColorIODisplayManager>())
{

}

void FOpenColorIOModule::StartupModule()
{
	FOpenColorIOLibHandler::Initialize();

	// Maps virtual shader source directory /Plugin/OpenCVLensDistortion to the plugin's actual Shaders directory.
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("OpenColorIO"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/OpenColorIO"), PluginShaderDir);
}

void FOpenColorIOModule::ShutdownModule()
{
	FOpenColorIOLibHandler::Shutdown();
}

FOpenColorIODisplayManager& FOpenColorIOModule::GetDisplayManager()
{
	return *DisplayManager;
}
	
IMPLEMENT_MODULE(FOpenColorIOModule, OpenColorIO);

#undef LOCTEXT_NAMESPACE
