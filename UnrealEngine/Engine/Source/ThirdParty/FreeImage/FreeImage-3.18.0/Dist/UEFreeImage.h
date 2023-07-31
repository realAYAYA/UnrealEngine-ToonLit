// Copyright Epic Games, Inc. All Rights Reserved.

THIRD_PARTY_INCLUDES_START
    #include "FreeImage.h"
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#   define TCHAR_TO_FICHAR TCHAR_TO_WCHAR
#   define FreeImage_GetFIFFromFilename FreeImage_GetFIFFromFilenameU
#   define FreeImage_GetFileType        FreeImage_GetFileTypeU
#   define FreeImage_Load               FreeImage_LoadU
#   define FreeImage_Save               FreeImage_SaveU
#else
#   define TCHAR_TO_FICHAR TCHAR_TO_UTF8
#endif

class FUEFreeImageWrapper
{
public:
	static bool IsValid() { return FreeImageDllHandle != nullptr; }

	static void FreeImage_Initialise(); // Loads and inits FreeImage on first call

private:
	static void* FreeImageDllHandle; // Lazy init on first use, never release for now
};

void* FUEFreeImageWrapper::FreeImageDllHandle = nullptr;

void FUEFreeImageWrapper::FreeImage_Initialise()
{
	if (FreeImageDllHandle != nullptr)
	{
		return;
	}

	// Push/PopDllDirectory are not threadsafe.
	// Must load library in main thread before doing parallel processing
	check(IsInGameThread());

	if (FreeImageDllHandle == nullptr)
	{
		FString FreeImageDir = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/FreeImage"), FPlatformProcess::GetBinariesSubdirectory());
		FString FreeImageLibDir = FPaths::Combine(FreeImageDir, TEXT(FREEIMAGE_LIB_FILENAME));
		FPlatformProcess::PushDllDirectory(*FreeImageDir);
		FreeImageDllHandle = FPlatformProcess::GetDllHandle(*FreeImageLibDir);
		FPlatformProcess::PopDllDirectory(*FreeImageDir);
	}

	if (FreeImageDllHandle)
	{
		FreeImage_Initialise();
	}
}