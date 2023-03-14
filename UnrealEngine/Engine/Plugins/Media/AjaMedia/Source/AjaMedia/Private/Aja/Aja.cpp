// Copyright Epic Games, Inc. All Rights Reserved.

#include "Aja.h"
#include "AjaMediaPrivate.h"

#include "Misc/FrameRate.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

 //~ Static initialization
 //--------------------------------------------------------------------
void* FAja::LibHandle = nullptr; 
bool FAja::bCanForceAJAUsage = false;

//~ Initialization functions implementation
//--------------------------------------------------------------------
bool FAja::Initialize()
{
#if AJAMEDIA_DLL_PLATFORM
	check(LibHandle == nullptr);

#if AJAMEDIA_DLL_DEBUG
	const FString AjaDll = TEXT("AJAd.dll");
#else
	const FString AjaDll = TEXT("AJA.dll");
#endif //AJAMEDIA_DLL_DEBUG

	// determine directory paths
	FString AjaDllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("AjaMedia"))->GetBaseDir(), TEXT("/Binaries/ThirdParty/Win64"));
	FPlatformProcess::PushDllDirectory(*AjaDllPath);
	AjaDllPath = FPaths::Combine(AjaDllPath, AjaDll);

	//Need to add the path to our dll dependency for cooked game, otherwise it won't be found when launching
	FString GPUTextureTransferDllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("MediaIOFramework"))->GetBaseDir(), TEXT("/Binaries/Win64"));
	FPlatformProcess::PushDllDirectory(*GPUTextureTransferDllPath);

	if (!FPaths::FileExists(AjaDllPath))
	{
		UE_LOG(LogAjaMedia, Error, TEXT("Failed to find the binary folder for the AJA dll. Plug-in will not be functional."));
		return false;
	}

	LibHandle = FPlatformProcess::GetDllHandle(*AjaDllPath);

	if (LibHandle == nullptr)
	{
		UE_LOG(LogAjaMedia, Error, TEXT("Failed to load required library %s. Plug-in will not be functional."), *AjaDllPath);
		return false;
	}

	//Check if command line argument to force AJA card usage is there
	bCanForceAJAUsage = FParse::Param(FCommandLine::Get(), TEXT("forceajausage"));

#if !NO_LOGGING
	AJA::SetLoggingCallbacks(&LogInfo, &LogWarning, &LogError);
#endif // !NO_LOGGING
	return true;
#else
	return false;
#endif // AJAMEDIA_DLL_PLATFORM
}

bool FAja::IsInitialized()
{
	return (LibHandle != nullptr);
}

void FAja::Shutdown()
{
#if AJAMEDIA_DLL_PLATFORM
	if (LibHandle != nullptr)
	{
#if !NO_LOGGING
		AJA::SetLoggingCallbacks(nullptr, nullptr, nullptr);
#endif // !NO_LOGGING
		FPlatformProcess::FreeDllHandle(LibHandle);
		LibHandle = nullptr;
	}
#endif // AJAMEDIA_DLL_PLATFORM
}

//~ Conversion functions implementation
//--------------------------------------------------------------------

FTimecode FAja::ConvertAJATimecode2Timecode(const AJA::FTimecode& InTimecode, const FFrameRate& InFPS)
{
	return FTimecode(InTimecode.Hours, InTimecode.Minutes, InTimecode.Seconds, InTimecode.Frames, InTimecode.bDropFrame);
}

//~ Log functions implementation
//--------------------------------------------------------------------
void FAja::LogInfo(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	UE_LOG(LogAjaMedia, Log, TempString);
#endif // !NO_LOGGING
}

void FAja::LogWarning(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	UE_LOG(LogAjaMedia, Warning, TempString);
#endif // !NO_LOGGING
}

void FAja::LogError(const TCHAR* InFormat, ...)
{
#if !NO_LOGGING
	TCHAR TempString[1024];
	va_list Args;

	va_start(Args, InFormat );
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	UE_LOG(LogAjaMedia, Error, TempString);
#endif // !NO_LOGGING
}



