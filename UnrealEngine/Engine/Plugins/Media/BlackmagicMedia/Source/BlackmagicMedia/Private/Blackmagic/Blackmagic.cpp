// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackmagic.h"
#include "BlackmagicMediaPrivate.h"

#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

/*
 * Static initialization
 */

void* FBlackmagic::LibHandle = nullptr;
bool FBlackmagic::bInitialized = false;
bool FBlackmagic::bCanForceBlackmagicUsage = false;

bool FBlackmagic::Initialize()
{
#if BLACKMAGICMEDIA_DLL_PLATFORM
	check(LibHandle == nullptr);

#if BLACKMAGICMEDIA_DLL_DEBUG
	const FString VideoIODll = TEXT("BlackmagicLibd.dll");
#else
	const FString VideoIODll = TEXT("BlackmagicLib.dll");
#endif // BLACKMAGICMEDIA_DLL_DEBUG

	// determine directory paths
	FString DllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("BlackmagicMedia"))->GetBaseDir(), TEXT("/Binaries/ThirdParty/Win64"));
	FPlatformProcess::PushDllDirectory(*DllPath);
	DllPath = FPaths::Combine(DllPath, VideoIODll);

	//Need to add the path to our dll dependency for cooked game, otherwise it won't be found when launching
	FString GPUTextureTransferDllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("MediaIOFramework"))->GetBaseDir(), TEXT("/Binaries/Win64"));
	FPlatformProcess::PushDllDirectory(*GPUTextureTransferDllPath);

	if (!FPaths::FileExists(DllPath))
	{
		UE_LOG(LogBlackmagicMedia, Error, TEXT("Failed to find the binary folder for the dll. Plug-in will not be functional."));
		return false;
	}

	LibHandle = FPlatformProcess::GetDllHandle(*DllPath);
	if (LibHandle == nullptr)
	{
		UE_LOG(LogBlackmagicMedia, Error, TEXT("Failed to load required library %s. Plug-in will not be functional."), *VideoIODll);
		return false;
	}

	//Check if command line argument to force Blackmagic card usage is there
	bCanForceBlackmagicUsage = FParse::Param(FCommandLine::Get(), TEXT("forceblackmagicusage"));

#if !NO_LOGGING
	BlackmagicDesign::SetLoggingCallbacks(&LogInfo, &LogWarning, &LogError, &LogVerbose);
#endif // !NO_LOGGING

	bInitialized = BlackmagicDesign::ApiInitialization();
	if (!bInitialized)
	{
		Shutdown();
	}

	return bInitialized;
#elif BLACKMAGICMEDIA_LINUX_PLATFORM
	//Check if command line argument to force Blackmagic card usage is there
	bCanForceBlackmagicUsage = FParse::Param(FCommandLine::Get(), TEXT("forceblackmagicusage"));

#if !NO_LOGGING
	BlackmagicDesign::SetLoggingCallbacks(&LogInfo, &LogWarning, &LogError, &LogVerbose);
#endif // !NO_LOGGING

	bInitialized = BlackmagicDesign::ApiInitialization();
	if (!bInitialized)
	{
		Shutdown();
	}

	return bInitialized;
#else
	return false;
#endif // BLACKMAGICMEDIA_DLL_PLATFORM
}

bool FBlackmagic::IsInitialized()
{
#if BLACKMAGICMEDIA_DLL_PLATFORM
	return LibHandle != nullptr && bInitialized;
#elif BLACKMAGICMEDIA_LINUX_PLATFORM
	return bInitialized;
#endif
}

void FBlackmagic::Shutdown()
{
	if (bInitialized)
#if BLACKMAGICMEDIA_DLL_PLATFORM
	{
		bInitialized = false;
		BlackmagicDesign::ApiUninitialization();
#if !NO_LOGGING
		BlackmagicDesign::SetLoggingCallbacks(nullptr, nullptr, nullptr, nullptr);
#endif // !NO_LOGGING
	}

	if (LibHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(LibHandle);
		LibHandle = nullptr;
	}
#elif BLACKMAGICMEDIA_LINUX_PLATFORM
	if (bInitialized)
	{
		bInitialized = false;
		BlackmagicDesign::ApiUninitialization();

#if !NO_LOGGING
		BlackmagicDesign::SetLoggingCallbacks(nullptr, nullptr, nullptr, nullptr);
#endif // !NO_LOGGING
	}
#endif // BLACKMAGICMEDIA_DLL_PLATFORM
}

void FBlackmagic::LogInfo(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	UE_LOG(LogBlackmagicMedia, Log, TempString);
#endif // !NO_LOGGIN
}

void FBlackmagic::LogWarning(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	UE_LOG(LogBlackmagicMedia, Warning, TempString);
#endif // !NO_LOGGIN
}

void FBlackmagic::LogError(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	UE_LOG(LogBlackmagicMedia, Error, TempString);
#endif // !NO_LOGGING
}

void FBlackmagic::LogVerbose(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat);
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	UE_LOG(LogBlackmagicMedia, Verbose, TempString);
#endif // !NO_LOGGING
}
