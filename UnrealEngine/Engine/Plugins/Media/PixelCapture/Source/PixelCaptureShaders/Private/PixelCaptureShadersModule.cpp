// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

class FPixelCaptureShadersModule : public IModuleInterface
{
public:
	virtual ~FPixelCaptureShadersModule() = default;

	virtual void StartupModule() override;
};

void FPixelCaptureShadersModule::StartupModule()
{
	FString ShaderDirectory = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("PixelCapture"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/PixelCapture"), ShaderDirectory);
}

IMPLEMENT_MODULE(FPixelCaptureShadersModule, PixelCaptureShaders)
