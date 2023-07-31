// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "OVR_Audio.h"
#include "CoreMinimal.h"
#include "Audio.h"
#include "Containers/Ticker.h"

//---------------------------------------------------
// Oculus Audio DLL handling
//---------------------------------------------------

static const TCHAR* GetOculusErrorString(ovrResult Result)
{
	switch (Result)
	{
		default:
		case ovrError_AudioUnknown:							return TEXT("Unknown Error");
		case ovrError_AudioInvalidParam:					return TEXT("Invalid Param");
		case ovrError_AudioBadSampleRate:					return TEXT("Bad Samplerate");
		case ovrError_AudioMissingDLL:						return TEXT("Missing DLL");
		case ovrError_AudioBadAlignment:					return TEXT("Pointers did not meet 16 byte alignment requirements");
		case ovrError_AudioUninitialized:					return TEXT("Function called before initialization");
		case ovrError_AudioHRTFInitFailure:					return TEXT("HRTF Profider initialization failed");
		case ovrError_AudioBadVersion:						return TEXT("Bad audio version");
		case ovrError_AudioSymbolNotFound:					return TEXT("DLL symbol not found");
		case ovrError_SharedReverbDisabled:					return TEXT("Shared reverb disabled");
		case ovrError_AudioNoAvailableAmbisonicInstance:	return TEXT("No available Ambisonic");\
	}
}

#define OVRA_CALL(Function) \
	[]() { typedef decltype(&Function) FunctionType; static FunctionType fp = (FunctionType)(FPlatformProcess::GetDllExport(FOculusAudioLibraryManager::Get().DllHandle(), TEXT(#Function))); return fp; }()


#define OVR_AUDIO_CHECK_RETURN_VALUE(Result, Context, FailureReturnValue)								\
	if (Result != ovrSuccess)																			\
	{																									\
		const TCHAR* ErrString = GetOculusErrorString(Result);											\
		UE_LOG(LogAudio, Error, TEXT("Oculus Audio SDK Error - %s: %s"), TEXT(Context), ErrString);		\
		return FailureReturnValue;																		\
	}

#define OVR_AUDIO_CHECK(Result, Context) OVR_AUDIO_CHECK_RETURN_VALUE(Result, Context, (void)0)

/************************************************************************/
/* FOculusAudioLibraryManager										   */
/* This handles loading and unloading the Oculus Audio DLL at runtime.  */
/************************************************************************/
class FOculusAudioLibraryManager
{
public:
	static FOculusAudioLibraryManager& Get();

	void Initialize();
	void Shutdown();

	bool IsInitialized() { return bInitialized; }
	ovrAudioContext GetPluginContext();

	void* DllHandle() { return OculusAudioDllHandle; }

private:
	FOculusAudioLibraryManager();
	virtual ~FOculusAudioLibraryManager();
	bool UpdatePluginContext(float DeltaTime);

	void* OculusAudioDllHandle;

	bool LoadDll();
	void ReleaseDll();

	uint32 NumInstances;
	bool bInitialized;
	ovrAudioContext CachedPluginContext;
	FTSTicker::FDelegateHandle TickDelegateHandle;
	uint32 ClientType;
};