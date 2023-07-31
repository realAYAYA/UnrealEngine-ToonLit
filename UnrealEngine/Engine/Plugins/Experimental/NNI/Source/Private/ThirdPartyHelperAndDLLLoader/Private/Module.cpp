// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ThirdPartyHelperAndDLLLoaderUtils.h"

// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
void FThirdPartyHelperAndDLLLoaderModule::StartupModule()
{
#ifdef WITH_DIRECTML
	const FString DirectMLRuntimeBinPath = FString(FPlatformProcess::BaseDir()) / TEXT(PREPROCESSOR_TO_STRING(DIRECTML_PATH));
	const FString DirectMLDLLPath = DirectMLRuntimeBinPath / TEXT("DirectML.dll");

	// Sanity check
	if (!FPaths::FileExists(DirectMLDLLPath))
	{
		const FString ErrorMessage = FString::Format(TEXT("DirectML DLL file not found in \"{0}\"."),
			{ IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*DirectMLDLLPath) });
		UE_LOG(LogNeuralNetworkInferenceThirdPartyHelperAndDLLLoader, Warning, TEXT("FThirdPartyHelperAndDLLLoaderModule::StartupModule(): %s"), *ErrorMessage);
		checkf(false, TEXT("%s"), *ErrorMessage);
	}

	FPlatformProcess::PushDllDirectory(*DirectMLRuntimeBinPath);
	DirectMLDLLHandle = FPlatformProcess::GetDllHandle(*DirectMLDLLPath);
	FPlatformProcess::PopDllDirectory(*DirectMLRuntimeBinPath);

#endif // WITH_DIRECTML
}

// This function may be called during shutdown to clean up your module. For modules that support dynamic reloading,
// we call this function before unloading the module.
void FThirdPartyHelperAndDLLLoaderModule::ShutdownModule()
{
#ifdef PLATFORM_WIN64
	if (DirectMLDLLHandle)
	{
		FPlatformProcess::FreeDllHandle(DirectMLDLLHandle);
		DirectMLDLLHandle = nullptr;
	}
#endif //PLATFORM_WIN64
}

IMPLEMENT_MODULE(FThirdPartyHelperAndDLLLoaderModule, ThirdPartyHelperAndDLLLoader);
