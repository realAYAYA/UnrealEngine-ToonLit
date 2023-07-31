// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <HAL/Thread.h>

THIRD_PARTY_INCLUDES_START

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#endif

#include <nvEncodeAPI.h>

#if PLATFORM_WINDOWS
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_END

DECLARE_LOG_CATEGORY_EXTERN(LogEncoderNVENC, Log, All);


// helper macro to define and clear NVENC structures
//  also sets the struct version number
#define NVENCStruct(OfType, VarName) \
	OfType	VarName;	\
	FMemory::Memzero(VarName);	\
	VarName.version = OfType ## _VER


namespace AVEncoder
{
	class FNVENCCommon : public NV_ENCODE_API_FUNCTION_LIST
	{
	public:
		// attempt to load NVENC 
		static FNVENCCommon& Setup();
		// shutdown - release loaded dll
		static void Shutdown();

		bool GetIsAvailable() const { return bIsAvailable; }

		FString GetErrorString(void* Encoder, NVENCSTATUS ForStatus)
		{
			const char* LastError = nvEncGetLastErrorString(Encoder);
			if (LastError)
			{
				return FString::Format(TEXT("error {0} ('{1}')"), { ForStatus, UTF8_TO_TCHAR(LastError) });
			}

			return FString("Error getting error string! Is NVENC configured correctly?");
		}

	private:
		FNVENCCommon() = default;

		void SetupNVENCFunctions();

		static FCriticalSection			ProtectSingleton;
		static FNVENCCommon				Singleton;
		void* DllHandle = nullptr;
		bool							bIsAvailable = false;
		bool							bWasSetUp = false;
	};

} /* namespace AVEncoder */