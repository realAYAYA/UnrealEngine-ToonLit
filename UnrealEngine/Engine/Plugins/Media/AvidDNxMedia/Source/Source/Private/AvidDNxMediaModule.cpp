// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvidDNxMediaModule.h"

#include "HAL/Platform.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "AvidDNx"

DEFINE_LOG_CATEGORY(LogAvidDNxMedia);

class FAvidDNxMediaModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		LibHandleDNx = LoadDll(TEXT("DNxHR.dll"));
		LibHandleMXF = LoadDll(TEXT("DNxMXF-dynamic.dll"));
		LibHandleDNxUncompressed = LoadDll(TEXT("DNxUncompressedSDK.dll"));
	}

	virtual void ShutdownModule() override
	{
		FreeDll(LibHandleDNx);
		FreeDll(LibHandleMXF);
		FreeDll(LibHandleDNxUncompressed);
	}

	void* LoadDll(const TCHAR* InDllName)
	{
		void* DllHandle = nullptr;

		// determine directory paths
		FString DllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("AvidDNxMedia"))->GetBaseDir(), TEXT("/Binaries/ThirdParty/Win64"));
		FPlatformProcess::PushDllDirectory(*DllPath);
		DllPath = FPaths::Combine(DllPath, InDllName);

		if (!FPaths::FileExists(DllPath))
		{
			UE_LOG(LogAvidDNxMedia, Error, TEXT("Failed to find the binary folder for %s. Plug-in will not be functional."), InDllName);
			return nullptr;
		}

		DllHandle = FPlatformProcess::GetDllHandle(*DllPath);
		if (!DllHandle)
		{
			UE_LOG(LogAvidDNxMedia, Error, TEXT("Failed to load required library %s. Plug-in will not be functional."), *DllPath);
		}

		return DllHandle;
	}

	void FreeDll(void* OutHandle)
	{
		if (OutHandle)
		{
			FPlatformProcess::FreeDllHandle(OutHandle);
			OutHandle = nullptr;
		}
	}

	// Codec could still be in use
	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

private:
	void* LibHandleDNx = nullptr;
	void* LibHandleMXF = nullptr;
	void* LibHandleDNxUncompressed = nullptr;
};

IMPLEMENT_MODULE(FAvidDNxMediaModule, AvidDNxMedia)

#undef LOCTEXT_NAMESPACE
