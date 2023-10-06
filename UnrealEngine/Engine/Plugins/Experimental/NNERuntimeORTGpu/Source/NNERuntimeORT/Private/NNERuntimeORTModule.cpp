// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTModule.h"
#include "NNE.h"
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

namespace UE::NNERuntimeORT::Private::DllHelper
{
	bool GetDllHandle(const FString& DllPath, TArray<void*>& DllHandles)
	{
		void *DllHandle = nullptr;

		if (!FPaths::FileExists(DllPath))
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to find the third party library %s."), *DllPath);
			return false;
		}
		
		DllHandle = FPlatformProcess::GetDllHandle(*DllPath);

		if (!DllHandle)
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to load the third party library %s."), *DllPath);
			return false;
		}

		DllHandles.Add(DllHandle);
		return true;
	}
}

void FNNERuntimeORTModule::StartupModule()
{
#if PLATFORM_WINDOWS
	const FString PluginDir = IPluginManager::Get().FindPlugin("NNERuntimeORTGpu")->GetBaseDir();
	const FString OrtBinPath = FPaths::Combine(PluginDir, TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIME_PLATFORM_PATH)));
	bool bAreDllsLoaded = true;

	bAreDllsLoaded &= UE::NNERuntimeORT::Private::DllHelper::GetDllHandle(FPaths::Combine(OrtBinPath, TEXT("onnxruntime.dll")), DllHandles);
	bAreDllsLoaded &= UE::NNERuntimeORT::Private::DllHelper::GetDllHandle(FPaths::Combine(OrtBinPath, TEXT("onnxruntime_providers_shared.dll")), DllHandles);
	//Note: onnxruntime_providers_cuda.dll should not be loaded explicitly. ORT will however load it from the same path onnxruntime_providers_shared.dll was loaded from.

	if (!bAreDllsLoaded)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to load OnnxRuntime Dlls. ORT Runtimes won't be available."));
		return;
	}

	Ort::InitApi();

	// NNE runtime ORT Cuda startup
	NNERuntimeORTCuda = NewObject<UNNERuntimeORTGpuImpl>();
	if (NNERuntimeORTCuda.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCudaInterface(NNERuntimeORTCuda.Get());

		NNERuntimeORTCuda->Init(ENNERuntimeORTGpuProvider::Cuda);
		NNERuntimeORTCuda->AddToRoot();
		UE::NNE::RegisterRuntime(RuntimeCudaInterface);
	}
	
	// NNE runtime ORT Dml startup
	NNERuntimeORTDml = NewObject<UNNERuntimeORTGpuImpl>();
	if (NNERuntimeORTDml.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDml.Get());

		NNERuntimeORTDml->Init(ENNERuntimeORTGpuProvider::Dml);
		NNERuntimeORTDml->AddToRoot();
		UE::NNE::RegisterRuntime(RuntimeDmlInterface);
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

		UE::NNE::UnregisterRuntime(RuntimeDmlInterface);
		NNERuntimeORTDml->RemoveFromRoot();
		NNERuntimeORTDml = TWeakObjectPtr<UNNERuntimeORTGpuImpl>(nullptr);
	}

	// NNE runtime ORT Cuda shutdown
	if (NNERuntimeORTCuda.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCudaInterface(NNERuntimeORTCuda.Get());

		UE::NNE::UnregisterRuntime(RuntimeCudaInterface);
		NNERuntimeORTCuda->RemoveFromRoot();
		NNERuntimeORTCuda = TWeakObjectPtr<UNNERuntimeORTGpuImpl>(nullptr);
	}

	// Free the dll handles
	for(void* DllHandle : DllHandles)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
	}
	DllHandles.Empty();
	
#endif
}

IMPLEMENT_MODULE(FNNERuntimeORTModule, NNERuntimeORT);