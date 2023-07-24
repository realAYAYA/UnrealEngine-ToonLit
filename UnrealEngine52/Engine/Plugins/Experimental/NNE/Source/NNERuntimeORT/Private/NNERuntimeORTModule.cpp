// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTModule.h"
#include "NNECore.h"
#include "NNERuntimeORT.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "UObject/WeakInterfacePtr.h"

#include "NNEThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "core/session/onnxruntime_cxx_api.h"
NNE_THIRD_PARTY_INCLUDES_END

void FNNERuntimeORTModule::StartupModule()
{
#if PLATFORM_WINDOWS
	const FString PluginDir = IPluginManager::Get().FindPlugin("NNE")->GetBaseDir();
	const FString OrtBinPath = FPaths::Combine(PluginDir, TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIME_PLATFORM_PATH)));
	const FString OrtLibPath = FPaths::Combine(OrtBinPath, TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIME_DLL_NAME)));

	if (!FPaths::FileExists(OrtLibPath))
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to find the third party library %s. Plug-in will not be functional."), *OrtLibPath);
		return;
	}

	{
		// FPlatformProcess::PushDllDirectory(*OrtBinPath);

		OrtLibHandle = FPlatformProcess::GetDllHandle(*OrtLibPath);

		// FPlatformProcess::PopDllDirectory(*OrtBinPath);
	}

	if (!OrtLibHandle)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to load the third party library %s. Plug-in will not be functional."), *OrtLibPath);
		return;
	}

	Ort::InitApi();
	
	// NNE runtime ORT Dml startup
	NNERuntimeORTDml = NewObject<UNNERuntimeORTDmlImpl>();
	if (NNERuntimeORTDml.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDml.Get());

		NNERuntimeORTDml->Init();
		NNERuntimeORTDml->AddToRoot();
		UE::NNECore::RegisterRuntime(RuntimeDmlInterface);
	}
#endif
}

void FNNERuntimeORTModule::ShutdownModule()
{
#if PLATFORM_WINDOWS
	// NNE runtime ORT Dml shutdown
	if (NNERuntimeORTDml.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDml.Get());

		UE::NNECore::UnregisterRuntime(RuntimeDmlInterface);
		NNERuntimeORTDml->RemoveFromRoot();
		NNERuntimeORTDml = TWeakObjectPtr<UNNERuntimeORTDmlImpl>(nullptr);
	}

	// Free the dll handle
	if (OrtLibHandle)
	{
		FPlatformProcess::FreeDllHandle(OrtLibHandle);
		OrtLibHandle = nullptr;
	}
#endif
}

IMPLEMENT_MODULE(FNNERuntimeORTModule, NNERuntimeORT);