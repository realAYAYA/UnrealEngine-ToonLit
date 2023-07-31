// Copyright Epic Games, Inc. All Rights Reserved.

#include "TencentDllMgr.h"
#include "OnlineSubsystemTencentPrivate.h"
#include "Misc/Paths.h"

FTencentDll::FTencentDll()
	: DllHandle(nullptr)
{
}

FTencentDll::~FTencentDll()
{
	Unload();
}

void* FTencentDll::Load(const FString& DllPath, const FString& DllFile)
{
	FString TencentSdkDllPath = DllPath;

#if PLATFORM_WINDOWS
	#if PLATFORM_32BITS
		TencentSdkDllPath = FPaths::Combine(*TencentSdkDllPath, TEXT("Win32"));
	#elif PLATFORM_64BITS
		TencentSdkDllPath = FPaths::Combine(*TencentSdkDllPath, TEXT("Win64"));
	#endif
#elif PLATFORM_LINUX
	#if PLATFORM_32BITS
		TencentSdkDllPath = FPaths::Combine(*TencentSdkDllPath, TEXT("linux32"));
	#elif PLATFORM_64BITS
		TencentSdkDllPath = FPaths::Combine(*TencentSdkDllPath, TEXT("linux64"));
	#endif
#endif

	DllHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(*TencentSdkDllPath, *DllFile));
	if (DllHandle == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not load Tencent dll: %s in %s"), *DllFile, *TencentSdkDllPath);
	}
	return DllHandle;
}

void FTencentDll::Unload()
{
	if (DllHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
		DllHandle = nullptr;
	}
}
