// Copyright Epic Games, Inc. All Rights Reserved.

#include "MobileFSRModule.h"

#include "CoreMinimal.h"
#include "ShaderCore.h"
#include "Interfaces/IPluginManager.h"
#include "MobileFSRViewExtension.h"

#define LOCTEXT_NAMESPACE "MobileFSR"

DEFINE_LOG_CATEGORY(LogMobileFSR);

void FMobileFSRModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("MobileFSR"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/MobileFSR"), PluginShaderDir);

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FMobileFSRModule::OnPostEngineInit);

	UE_LOG(LogMobileFSR, Log, TEXT("Mobile FSR Started"));
}

void FMobileFSRModule::OnPostEngineInit()
{
	ViewExtension = FSceneViewExtensions::NewExtension<FMobileFSRViewExtension>();

	UE_LOG(LogMobileFSR, Log, TEXT("Mobile FSR OnPostEngineInit"));
}

void FMobileFSRModule::ShutdownModule()
{
	ViewExtension = nullptr;
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	UE_LOG(LogMobileFSR, Log, TEXT("Mobile FSR Shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMobileFSRModule, MobileFSR)