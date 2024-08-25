// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCachingUSD/Module.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "UnrealUSDWrapper.h"
#include "USDMemory.h"

IMPLEMENT_MODULE_USD(FChaosCachingUSDModule, ChaosCachingUSD);

DEFINE_LOG_CATEGORY(LogChaosCacheUSD)

void FChaosCachingUSDModule::StartupModule()
{
#if USE_USD_SDK
	// Register the ChaosCachingUSD plugin with USD.
	IPluginManager& UEPluginManager = IPluginManager::Get();
	FString USDImporterDir = UEPluginManager.FindPlugin(TEXT("USDImporter"))->GetBaseDir();

	const FString ChaosCachingUSDResourcesDir =
		FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				USDImporterDir, 
				FString(TEXT("ChaosCachingUSD")),
				FString(TEXT("Resources"))));

	UnrealUSDWrapper::RegisterPlugins(ChaosCachingUSDResourcesDir);
#endif // USE_USD_SDK
}
