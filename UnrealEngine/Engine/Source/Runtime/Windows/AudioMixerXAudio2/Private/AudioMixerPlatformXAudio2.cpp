// Copyright Epic Games, Inc. All Rights Reserved.

/**
	Concrete implementation of FAudioDevice for XAudio2

	See https://msdn.microsoft.com/en-us/library/windows/desktop/hh405049%28v=vs.85%29.aspx
*/

#include "AudioMixerPlatformXAudio2.h"
#include "AudioMixer.h"
#include "AudioDevice.h"
#include "HAL/PlatformAffinity.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Engine/EngineTypes.h"

#ifndef WITH_XMA2
#define WITH_XMA2 0
#endif
#if WITH_ENGINE
#if WITH_XMA2
#include "XMAAudioInfo.h"
#endif  //#if WITH_XMA2
#endif //WITH_ENGINE
#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopeLock.h"
#include "HAL/Event.h"
#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "ToStringHelpers.h"

#if PLATFORM_WINDOWS
THIRD_PARTY_INCLUDES_START
#include <mmdeviceapi.h>
#include <FunctionDiscoveryKeys_devpkey.h>
#include <AudioClient.h>
THIRD_PARTY_INCLUDES_END
#endif

#include "Misc/CoreDelegates.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Async/Async.h"

#define XAUDIO2_LOG_RESULT(FunctionName, Result) \
	{ \
		FString ErrorString = FString::Printf(TEXT("%s -> 0x%X: %s (line: %d)"), TEXT( FunctionName ), Result, *Audio::ToErrorFString(Result), __LINE__); \
		UE_LOG(LogAudioMixer, Error, TEXT("XAudio2 Error: %s"), *ErrorString);																		\
	}

// Macro to check result code for XAudio2 failure, get the string version, log, and goto a cleanup
#define XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result)																										 \
	if (FAILED(Result))																														 \
	{																																		 \
		FString ErrorString = FString::Printf(TEXT("%s -> 0x%X: %s (line: %d)"), TEXT( #Result ), Result, *Audio::ToErrorFString(Result), __LINE__);\
		UE_LOG(LogAudioMixer, Error, TEXT("XAudio2 Error: %s"), *ErrorString);																 \
		goto Cleanup;																														 \
	}

// Macro to check result for XAudio2 failure, get string version, log, and return false
#define XAUDIO2_RETURN_ON_FAIL(Result)																										 \
	if (FAILED(Result))																														 \
	{																																		 \
		FString ErrorString = FString::Printf(TEXT("%s -> 0x%X: %s (line: %d)"), TEXT( #Result ), Result, *Audio::ToErrorFString(Result), __LINE__);\
		UE_LOG(LogAudioMixer, Error, TEXT("XAudio2 Error: %s"), *ErrorString);																 \
		return false;																														 \
	}

static XAUDIO2_PROCESSOR GetXAudio2ProcessorsToUse()
{
	XAUDIO2_PROCESSOR ProcessorsToUse = (XAUDIO2_PROCESSOR)FPlatformAffinity::GetAudioRenderThreadMask();
	// https://docs.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-xaudio2create
	// Warning If you specify XAUDIO2_ANY_PROCESSOR, the system will use all of the device's processors and, as noted above, create a worker thread for each processor.
	// We certainly don't want to use all available CPU. XAudio threads are time critical priority and wake up every 10 ms, they may cause lots of unwarranted context switches.
	// In case no specific affinity is specified, let XAudio choose the default processor. It should allocate a single thread and should be enough.
	if (ProcessorsToUse == XAUDIO2_ANY_PROCESSOR)
	{
	#ifdef XAUDIO2_USE_DEFAULT_PROCESSOR
		ProcessorsToUse = XAUDIO2_USE_DEFAULT_PROCESSOR;
	#else
		ProcessorsToUse = XAUDIO2_DEFAULT_PROCESSOR;
	#endif
	}

	return ProcessorsToUse;
}

#if PLATFORM_WINDOWS 
FName GetDllName(FName Current = NAME_None) 
{
#if PLATFORM_64BITS
	static const FString XAudio2_9Redist = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Windows/XAudio2_9/x64/xaudio2_9redist.dll");
#else
	static const FString XAudio2_9Redist = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Windows/XAudio2_9/x86/xaudio2_9redist.dll");
#endif
	return *XAudio2_9Redist;
}
#endif //#if PLATFORM_WINDOWS 

/*
	Whether or not to enable xaudio2 debugging mode
	To see the debug output, you need to view ETW logs for this application:
	Go to Control Panel, Administrative Tools, Event Viewer.
	View->Show Analytic and Debug Logs.
	Applications and Services Logs / Microsoft / Windows / XAudio2.
	Right click on Microsoft Windows XAudio2 debug logging, Properties, then Enable Logging, and hit OK
*/
#define XAUDIO2_DEBUG_ENABLED 0

namespace Audio
{
	void FXAudio2VoiceCallback::OnBufferEnd(void* BufferContext)
	{
		SCOPED_NAMED_EVENT(FXAudio2VoiceCallback_OnBufferEnd, FColor::Blue);
		
		check(BufferContext);
		IAudioMixerPlatformInterface* MixerPlatform = (IAudioMixerPlatformInterface*)BufferContext;
		MixerPlatform->ReadNextBuffer();
	}

	static uint32 ChannelTypeMap[EAudioMixerChannel::ChannelTypeCount] =
	{
		SPEAKER_FRONT_LEFT,
		SPEAKER_FRONT_RIGHT,
		SPEAKER_FRONT_CENTER,
		SPEAKER_LOW_FREQUENCY,
		SPEAKER_BACK_LEFT,
		SPEAKER_BACK_RIGHT,
		SPEAKER_FRONT_LEFT_OF_CENTER,
		SPEAKER_FRONT_RIGHT_OF_CENTER,
		SPEAKER_BACK_CENTER,
		SPEAKER_SIDE_LEFT,
		SPEAKER_SIDE_RIGHT,
		SPEAKER_TOP_CENTER,
		SPEAKER_TOP_FRONT_LEFT,
		SPEAKER_TOP_FRONT_CENTER,
		SPEAKER_TOP_FRONT_RIGHT,
		SPEAKER_TOP_BACK_LEFT,
		SPEAKER_TOP_BACK_CENTER,
		SPEAKER_TOP_BACK_RIGHT,
		SPEAKER_RESERVED,
	};

	FMixerPlatformXAudio2::FMixerPlatformXAudio2()
		: XAudio2Dll(nullptr)
		, bDeviceChanged(false)
		, XAudio2System(nullptr)
		, OutputAudioStreamMasteringVoice(nullptr)
		, OutputAudioStreamSourceVoice(nullptr)
		, LastDeviceSwapTime(0.0)
		, TimeSinceNullDeviceWasLastChecked(0.0f)		
		, bIsInitialized(false)
		, bIsDeviceOpen(false)
		, bIsSuspended(false)
	{
#if PLATFORM_WINDOWS 
		FPlatformMisc::CoInitialize();
		DllName = GetDllName();
#endif // #if PLATFORM_WINDOWS 
	}

	FMixerPlatformXAudio2::~FMixerPlatformXAudio2()
	{
#if PLATFORM_WINDOWS 
		FPlatformMisc::CoUninitialize();
#endif // #if PLATFORM_WINDOWS 
	}

#if PLATFORM_WINDOWS
	// Dirty extern for now.
	extern void RegisterForSessionEvents(const FString& InDeviceId);
#endif //PLATFORM_WINDOWS

	bool FMixerPlatformXAudio2::CheckThreadedDeviceSwap()
	{
		bool bDidStopGeneratingAudio = false;
#if PLATFORM_WINDOWS
		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_CheckThreadedDeviceSwap, FColor::Blue);

		static float ThreadedSwapDebugExtraTimeMsCVar = 0;
		static FAutoConsoleVariableRef CVarOverrunTimeout(
			TEXT("au.ThreadedSwapDebugExtraTime"),
			ThreadedSwapDebugExtraTimeMsCVar,
			TEXT("Simulate a slow device swap by adding addional time to the swap task"),
			ECVF_Default);
	
		if (bMoveAudioStreamToNewAudioDevice || ActiveDeviceSwap.IsValid())
		{
			// Start a job?
			if (!ActiveDeviceSwap.IsValid())
			{
				SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_CheckThreadedDeviceSwap_StartAsyncSwap, FColor::Blue);

				UE_LOG(LogAudioMixer, Verbose, TEXT("FMixerPlatformXAudio2::CheckThreadedDeviceSwap() Starting swap to [%s]"), NewAudioDeviceId.IsEmpty() ? TEXT("[System Default]") : *NewAudioDeviceId);

				// Toggle move to audio stream now we're spawning a new job.
				bMoveAudioStreamToNewAudioDevice = false;

				// Look up device. Blank name looks up current default.
				FName NewDeviceName = *NewAudioDeviceId;
				FAudioPlatformDeviceInfo NewDevice;
				bool bSwitchingToValidDevice = false;
				check(GetDeviceInfoCache());
				if (TOptional<FAudioPlatformDeviceInfo> DeviceInfo = GetDeviceInfoCache()->FindActiveOutputDevice(NewDeviceName))
				{
					if (DeviceInfo->NumChannels <= XAUDIO2_MAX_AUDIO_CHANNELS &&
						DeviceInfo->SampleRate >= XAUDIO2_MIN_SAMPLE_RATE &&
						DeviceInfo->SampleRate <= XAUDIO2_MAX_SAMPLE_RATE)
					{
						bSwitchingToValidDevice = true;
						NewDevice = *DeviceInfo;
					}
					else
					{
						UE_LOG(LogAudioMixer, Warning, TEXT("Ignoring attempt to switch to device with unsupported params: Channels=%u, SampleRate=%u, Id=%s, Name=%s"),
							(uint32)DeviceInfo->NumChannels, (uint32)DeviceInfo->SampleRate, *DeviceInfo->DeviceId, *DeviceInfo->Name);

						NewDevice.Reset();
					}
				}
				else
				{
					// Failed to find any device/default or otherwise. Reset our current device and do a switch to a null device.
					NewDevice.Reset();
				}
				
				TFunction<FXAudio2AsyncCreateResult()> AsyncDeleteCreate;

				{
					// Lock for creation of lambda as we are copying state.
					FScopeLock Lock(&AudioDeviceSwapCriticalSection);

					// Build Delete/Create Async lambda
					AsyncDeleteCreate = [
						System = XAudio2System,
						SourceVoice = OutputAudioStreamSourceVoice,
						MasterVoice = OutputAudioStreamMasteringVoice,
						NewDevice,
						&Callbacks = OutputVoiceCallback,
						RenderingSampleRate = OpenStreamParams.SampleRate,
						bSwitchingToValidDevice,
						SwapReason = DeviceSwapReason
					]() mutable->FXAudio2AsyncCreateResult
						{
							uint64_t StartTimeCycles = FPlatformTime::Cycles64();
							
							SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate, FColor::Blue);
							UE_LOG(LogAudioMixer, Verbose, TEXT("FMixerPlatformXAudio2::CheckThreadedDeviceSwap() - AsyncTask Start. Because=%s"), *SwapReason);

							// New thread might not have COM setup.
							FPlatformMisc::CoInitialize();

							{
								// Stop old engine running.
								if (System)
								{
									SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_StopEngine, FColor::Blue);
									System->StopEngine();
								}

								// Kill source voice.
								if (SourceVoice)
								{
									{
										SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_FlushSourceBuffers, FColor::Blue);
										HRESULT Result = SourceVoice->FlushSourceBuffers();
										if (FAILED(Result))
										{
											XAUDIO2_LOG_RESULT("SourceVoice->FlushSourceBuffers", Result);
										}
									}
									SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_DestroySourceVoice, FColor::Blue);
									SourceVoice->DestroyVoice();
									SourceVoice = nullptr;
								}

								// Now destroy the mastering voice
								if (MasterVoice)
								{
									SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_DestroyMasterVoice, FColor::Blue);
									MasterVoice->DestroyVoice();
									MasterVoice = nullptr;
								}

								// Destroy System
								{
									SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_DestroySystem, FColor::Blue);
									SAFE_RELEASE(System);
								}
							}

							FXAudio2AsyncCreateResult Results;
							Results.SwapReason = SwapReason;

							// Don't attempt to create a new setup if there's no devices available.
							if (!bSwitchingToValidDevice)
							{
								return {};
							}

							// Create System.
							{
								SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_CreateSystem, FColor::Blue);
								HRESULT Result = XAudio2Create(&Results.XAudio2System, 0, GetXAudio2ProcessorsToUse());
								if (FAILED(Result))
								{
									XAUDIO2_LOG_RESULT("XAudio2Create", Result);
									return {};// FAIL.
								}
							}

							// Create Master
							{
								check(NewDevice.NumChannels <= XAUDIO2_MAX_AUDIO_CHANNELS);
								check(NewDevice.SampleRate >= XAUDIO2_MIN_SAMPLE_RATE);
								check(NewDevice.SampleRate <= XAUDIO2_MAX_SAMPLE_RATE);

								SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_CreateMasterVoice, FColor::Blue);
								HRESULT Result = Results.XAudio2System->CreateMasteringVoice(
									&Results.OutputAudioStreamMasteringVoice,
									(uint32)NewDevice.NumChannels,
									(uint32)NewDevice.SampleRate,
									0,
									*NewDevice.DeviceId,
									nullptr,
									AudioCategory_GameEffects
								);
								if (FAILED(Result))
								{
									UE_LOG(LogAudioMixer, Error, TEXT("XAudio2System->CreateMasteringVoice() -> 0x%X: %s (line: %d) with Args (NumChannels=%u, SampleRate=%u, DeviceID=%s Name=%s)"),
										Result, *Audio::ToErrorFString(Result), __LINE__, (uint32)NewDevice.NumChannels, (uint32)NewDevice.SampleRate, *NewDevice.DeviceId, *NewDevice.Name);
									SAFE_RELEASE(Results.XAudio2System);
									return {}; // FAIL.
								}
							}

							// Create Source Voice.
							{
								SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_CreateSourceVoice, FColor::Blue);

								// Setup the format of the output source voice
								WAVEFORMATEX Format = { 0 };
								Format.nChannels = NewDevice.NumChannels;
								Format.nSamplesPerSec = RenderingSampleRate;		// NOTE: We use the Rendering sample rate here.
								Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
								Format.nAvgBytesPerSec = Format.nSamplesPerSec * sizeof(float) * Format.nChannels;
								Format.nBlockAlign = sizeof(float) * Format.nChannels;
								Format.wBitsPerSample = sizeof(float) * 8;

								// Create the output source voice
								HRESULT Result = Results.XAudio2System->CreateSourceVoice(
									&Results.OutputAudioStreamSourceVoice,
									&Format,
									XAUDIO2_VOICE_NOPITCH,
									XAUDIO2_DEFAULT_FREQ_RATIO,
									&Callbacks,
									nullptr,
									nullptr
								);

								if (FAILED(Result))
								{
									XAUDIO2_LOG_RESULT("XAudio2System->CreateSourceVoice", Result);
									Results.OutputAudioStreamMasteringVoice->DestroyVoice();
									Results.OutputAudioStreamMasteringVoice = nullptr;
									SAFE_RELEASE(Results.XAudio2System);
									return {};
								}
							}

							// Optionally for testing, sleep for some duration in order to help repro race conditions.
							if (ThreadedSwapDebugExtraTimeMsCVar > 0.f)
							{
								FPlatformProcess::Sleep(ThreadedSwapDebugExtraTimeMsCVar/1000.f); 
							}

							// Listen session for changes to this device.
							RegisterForSessionEvents(NewDevice.DeviceId);

							FPlatformMisc::CoUninitialize();

							// Success.
							Results.SuccessfullDurationMs = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTimeCycles);
							Results.DeviceInfo = NewDevice;
							return Results;
						}; // End of Lambda.

						// Still inside CS here.
						// NULL our system / master / source voices. (as they are about to be torn down async).
						bIsInDeviceSwap = true;
						XAudio2System = nullptr;
						OutputAudioStreamMasteringVoice = nullptr;
						OutputAudioStreamSourceVoice = nullptr;
					} // End of Lambda Creation scope (with CS).

				
					{
						SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_StopSource, FColor::Blue);

						// Ask source voice to stop. 
						if (OutputAudioStreamSourceVoice)
						{
							HRESULT Hr = OutputAudioStreamSourceVoice->Stop();
							if (FAILED(Hr))
							{
								XAUDIO2_LOG_RESULT("OutputAudioStreamSourceVoice->Stop()", Hr);
							}
						}

					}

					// Start a new device swap.
					{
						SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_AsyncDeleteCreate_Async, FColor::Blue);
						ActiveDeviceSwap = Async(EAsyncExecution::TaskGraph, MoveTemp(AsyncDeleteCreate));
					}

					if (!bIsUsingNullDevice)
					{
						StartRunningNullDevice();
					}

					// Return FALSE, as null can run with whatever the same config we were using before.
					return false;
			}
			else // Running...
			{
				if (ActiveDeviceSwap.IsReady())  // Finished?
				{
					SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_CheckThreadedDeviceSwap_FinishedJobSwitchOver, FColor::Blue);

					XAudio2System = ActiveDeviceSwap.Get().XAudio2System;
					OutputAudioStreamMasteringVoice = ActiveDeviceSwap.Get().OutputAudioStreamMasteringVoice;
					OutputAudioStreamSourceVoice = ActiveDeviceSwap.Get().OutputAudioStreamSourceVoice;

					// Success?
					if (XAudio2System && OutputAudioStreamSourceVoice && OutputAudioStreamMasteringVoice)
					{
						HRESULT Result;
						{
							SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_CheckThreadedDeviceSwap_StartEngine, FColor::Blue);
							Result = XAudio2System->StartEngine();
						}
						if (SUCCEEDED(Result))
						{
							if (bIsUsingNullDevice)
							{
								StopRunningNullDevice();
							}

							StopGeneratingAudio();
							bDidStopGeneratingAudio = true;

							// Copy our new Device Info into our active one.
							AudioStreamInfo.DeviceInfo = ActiveDeviceSwap.Get().DeviceInfo;

							// Display our new XAudio2 Mastering voice details.
							UE_LOG(LogAudioMixer, Display, TEXT("Successful Swap new Device is (NumChannels=%u, SampleRate=%u, DeviceID=%s, Name=%s), Reason=%s, InstanceID=%d, DurationMS=%.2f"),
								(uint32)AudioStreamInfo.DeviceInfo.NumChannels, (uint32)AudioStreamInfo.DeviceInfo.SampleRate, *AudioStreamInfo.DeviceInfo.DeviceId, *AudioStreamInfo.DeviceInfo.Name, *ActiveDeviceSwap.Get().SwapReason, InstanceID, ActiveDeviceSwap.Get().SuccessfullDurationMs);

							// Reinitialize the output circular buffer to match the buffer math of the new audio device.
							const int32 NumOutputSamples = AudioStreamInfo.NumOutputFrames * AudioStreamInfo.DeviceInfo.NumChannels;
							if (ensure(NumOutputSamples > 0))
							{
								OutputBuffer.Init(AudioStreamInfo.AudioMixer, NumOutputSamples, NumOutputBuffers, AudioStreamInfo.DeviceInfo.Format);
							}
						}
						else
						{
							XAUDIO2_LOG_RESULT("StartEngine", Result);
						}
					}
					else // We either failed to init or deliberately switched to null device.
					{
						// Null renderer doesn't/shouldn't care about the format, so leave the format as it was before.
					}

					{
						SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_CheckThreadedDeviceSwap_EndSwap, FColor::Blue);
						bIsInDeviceSwap = false;
						ActiveDeviceSwap.Reset();
					}
				}
			}
		}
#endif //PLATFORM_WINDOWS

		return bDidStopGeneratingAudio;
	}

	bool FMixerPlatformXAudio2::AllowDeviceSwap()
	{
#if PLATFORM_WINDOWS
		double CurrentTime = FPlatformTime::Seconds();

		// If we're already in the process of swapping, we do not want to "double-trigger" a swap
		if (bMoveAudioStreamToNewAudioDevice)
		{
			LastDeviceSwapTime = CurrentTime;
			return false;
		}

		// Some devices spam device swap notifications, so we want to rate-limit them to prevent double/triple triggering.
		static const int32 MinSwapTimeMs = 10;
		if (CurrentTime - LastDeviceSwapTime > (double)MinSwapTimeMs / 1000.0)
		{
			LastDeviceSwapTime = CurrentTime;
			return true;
		}
#endif
		return false;
	}

	bool FMixerPlatformXAudio2::ResetXAudio2System()
	{
		SAFE_RELEASE(XAudio2System);

		uint32 Flags = 0;

#if WITH_XMA2
		// We need to raise this flag explicitly to prevent initializing SHAPE twice, because we are allocating SHAPE in FXMAAudioInfo
		Flags |= XAUDIO2_DO_NOT_USE_SHAPE;
#endif

		if (FAILED(XAudio2Create(&XAudio2System, Flags, GetXAudio2ProcessorsToUse())))
		{
			XAudio2System = nullptr;
			return false;
		}

		return true;
	}

	void FMixerPlatformXAudio2::Suspend()
	{
		SCOPED_ENTER_BACKGROUND_EVENT(STAT_FMixerPlatformXAudio2_Suspend);
		if( !bIsSuspended )
		{				
			if( XAudio2System )
			{
				XAudio2System->StopEngine();
				StartRunningNullDevice();
				bIsSuspended = true;
			}					
		}
	}
	void FMixerPlatformXAudio2::Resume()
	{
		if( bIsSuspended )
		{
			if( XAudio2System )
			{			
				StopRunningNullDevice();			

				HRESULT Result = XAudio2System->StartEngine();				
				
				// Suggestion from Microsoft for this returning 0x80070490 on a quick cycle. 
				// Try again with delay.
				constexpr int32 NumStartAttempts = 16;
				constexpr float DelayBetweenAttemptsSecs = 0.1f;
				
				uint64 StartCycles = FPlatformTime::Cycles64();				
				int32 StartAttempt = 0;
				for(; StartAttempt < NumStartAttempts && FAILED(Result); ++StartAttempt)
				{
					FPlatformProcess::Sleep(DelayBetweenAttemptsSecs);
					Result = XAudio2System->StartEngine();	
				}

				UE_CLOG(FAILED(Result), LogAudioMixer, Error,
					TEXT("Could not resume XAudio2, StartEngine() returned %#010x, after %d attempts"),
					(uint32)Result, StartAttempt
				);
				
				if(SUCCEEDED(Result))
				{		
					bIsSuspended = false;								
					UE_CLOG(StartAttempt > 1, LogAudioMixer, Warning, 
						TEXT("StartEngine() took %d attempts to start, taking '%f' ms"), 
						StartAttempt, FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles));
				}
			}						
		}		
	}

	bool FMixerPlatformXAudio2::InitializeHardware()
	{
		if (bIsInitialized)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("XAudio2 already initialized."), Warning);
			return false;

		}

#if PLATFORM_NEEDS_SUSPEND_ON_BACKGROUND

		// Constrain/Suspend 
		DeactiveHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FMixerPlatformXAudio2::Suspend);
		ReactivateHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FMixerPlatformXAudio2::Resume);
		EnteredBackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FMixerPlatformXAudio2::Suspend);
		EnteredForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FMixerPlatformXAudio2::Resume);

#endif //PLATFORM_NEEDS_SUSPEND_ON_BACKGROUND
		
#if PLATFORM_WINDOWS
		// Work around the fact the x64 version of XAudio2_7.dll does not properly ref count
		// by forcing it to be always loaded

		// Load the xaudio2 library and keep a handle so we can free it on teardown
		// Note: windows internally ref-counts the library per call to load library so 
		// when we call FreeLibrary, it will only free it once the refcount is zero
		// Also, FPlatformProcess::GetDllHandle should not be used, as it will not increase ref count further if the library is already loaded.
		// FPaths::ConvertRelativePathToFull is used for parity with how GetDllHandle calls LoadLibrary.
		XAudio2Dll = LoadLibrary(*FPaths::ConvertRelativePathToFull(DllName.GetPlainNameString()));

		// returning null means we failed to load XAudio2, which means everything will fail
		if (XAudio2Dll == nullptr)
		{
			UE_LOG(LogInit, Warning, TEXT("Failed to load XAudio2 dll"));
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Audio", "XAudio2Missing", "XAudio2.7 is not installed. Make sure you have XAudio 2.7 installed. XAudio 2.7 is available in the DirectX End-User Runtime (June 2010)."));
			return false;
		}
#endif // #if PLATFORM_WINDOWS

		uint32 Flags = 0;

#if WITH_XMA2
		// We need to raise this flag explicitly to prevent initializing SHAPE twice, because we are allocating SHAPE in FXMAAudioInfo
		Flags |= XAUDIO2_DO_NOT_USE_SHAPE;
#endif

		if (!XAudio2System && FAILED(XAudio2Create(&XAudio2System, Flags, GetXAudio2ProcessorsToUse())))
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Audio", "XAudio2Error", "Failed to initialize audio. This may be an issue with your installation of XAudio 2.7. XAudio2 is available in the DirectX End-User Runtime (June 2010)."));
			return false;
		}
#if XAUDIO2_DEBUG_ENABLED
		XAUDIO2_DEBUG_CONFIGURATION DebugConfiguration = { 0 };
		DebugConfiguration.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
		XAudio2System->SetDebugConfiguration(&DebugConfiguration, 0);
#endif // #if XAUDIO2_DEBUG_ENABLED

#if WITH_XMA2
		//Initialize our XMA2 decoder context
		XMA2_INFO_CALL(FXMAAudioInfo::Initialize());
#endif //#if WITH_XMA2

		if(IAudioMixer::ShouldRecycleThreads())
		{
			// Pre-create the null render device thread on XAudio2, so we can simple wake it up when we need it.
			// Give it nothing to do, with a slow tick as the default, but ask it to wait for a signal to wake up.
			CreateNullDeviceThread([] {}, 1.0f, true);
		}

		bIsInitialized = true;

		return true;
	}

	bool FMixerPlatformXAudio2::TeardownHardware()
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("XAudio2 was already tore down."), Warning);
			return false;
		}

#if PLATFORM_NEEDS_SUSPEND_ON_BACKGROUND

		auto UnregisterLambda = [](FDelegateHandle& InHandle, TMulticastDelegate<void()>& InDelegate)
		{
			if (InHandle.IsValid())
			{
				InDelegate.Remove(InHandle);
				InHandle.Reset();
			}
		};
	 		
		UnregisterLambda(DeactiveHandle,FCoreDelegates::ApplicationWillDeactivateDelegate);
		UnregisterLambda(ReactivateHandle,FCoreDelegates::ApplicationHasReactivatedDelegate);
		UnregisterLambda(EnteredBackgroundHandle,FCoreDelegates::ApplicationWillEnterBackgroundDelegate);
		UnregisterLambda(EnteredForegroundHandle,FCoreDelegates::ApplicationHasEnteredForegroundDelegate);

#endif //PLATFORM_NEEDS_SUSPEND_ON_BACKGROUND

		SAFE_RELEASE(XAudio2System);

#if WITH_XMA2
		XMA2_INFO_CALL(FXMAAudioInfo::Shutdown());
#endif

#if PLATFORM_WINDOWS
		if (XAudio2Dll != nullptr && IsEngineExitRequested())
		{
			if (!FreeLibrary(XAudio2Dll))
			{
				UE_LOG(LogAudio, Warning, TEXT("Failed to free XAudio2 Dll"));
			}

			XAudio2Dll = nullptr;
		}
#endif
		bIsInitialized = false;

		return true;
	}

	bool FMixerPlatformXAudio2::IsInitialized() const
	{
		return bIsInitialized;
	}

	bool FMixerPlatformXAudio2::GetNumOutputDevices(uint32& OutNumOutputDevices)
	{
		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_GetNumOutputDevices, FColor::Blue);

		// Use Cache if we have it.
		if (IAudioPlatformDeviceInfoCache* Cache = GetDeviceInfoCache())
		{
			OutNumOutputDevices = Cache->GetAllActiveOutputDevices().Num();
			return true;
		}

		OutNumOutputDevices = 0;

		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("XAudio2 was not initialized."), Error);
			return false;
		}

		// XAudio2 for HoloLens doesn't have GetDeviceCount, use Windows::Devices::Enumeration instead
		// See https://blogs.msdn.microsoft.com/chuckw/2012/04/02/xaudio2-and-windows-8/
#if PLATFORM_WINDOWS && XAUDIO_SUPPORTS_DEVICE_DETAILS

		IMMDeviceEnumerator* DeviceEnumerator = nullptr;
		IMMDeviceCollection* DeviceCollection = nullptr;

		HRESULT Result = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&DeviceEnumerator));
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		Result = DeviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &DeviceCollection);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		uint32 DeviceCount;
		Result = DeviceCollection->GetCount(&DeviceCount);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		OutNumOutputDevices = DeviceCount;

	Cleanup:
		SAFE_RELEASE(DeviceCollection);
		SAFE_RELEASE(DeviceEnumerator);

		return SUCCEEDED(Result);
#else
		OutNumOutputDevices = 1;
		return true;
#endif 
	}

#if PLATFORM_WINDOWS
	static bool GetMMDeviceInfo(IMMDevice* MMDevice, FAudioPlatformDeviceInfo& OutInfo)
	{
		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_GetMMDeviceInfo, FColor::Blue);
		
		check(MMDevice);

		OutInfo.Reset();

		IPropertyStore *PropertyStore = nullptr;
		WAVEFORMATEX* WaveFormatEx = nullptr;
		PROPVARIANT FriendlyName;
		PROPVARIANT DeviceFormat;
		LPWSTR DeviceId;

		check(MMDevice);
		PropVariantInit(&FriendlyName);
		PropVariantInit(&DeviceFormat);

		// Get the device id
		HRESULT Result = MMDevice->GetId(&DeviceId);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		// Open up the property store so we can read properties from the device
		Result = MMDevice->OpenPropertyStore(STGM_READ, &PropertyStore);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		// Grab the friendly name
		PropVariantInit(&FriendlyName);
		Result = PropertyStore->GetValue(PKEY_Device_FriendlyName, &FriendlyName);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		OutInfo.Name = FString(FriendlyName.pwszVal);

		// Retrieve the DeviceFormat prop variant
		Result = PropertyStore->GetValue(PKEY_AudioEngine_DeviceFormat, &DeviceFormat);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		// Get the format of the property
		WaveFormatEx = (WAVEFORMATEX *)DeviceFormat.blob.pBlobData;
		if (!WaveFormatEx)
		{
			// Some devices don't provide the Device format, so try the OEMFormat as well.
			Result = PropertyStore->GetValue(PKEY_AudioEngine_OEMFormat, &DeviceFormat);
			XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

			WaveFormatEx = (WAVEFORMATEX*)DeviceFormat.blob.pBlobData;
			if (!ensure(DeviceFormat.blob.pBlobData))
			{
				// Force an error if we failed to get a WaveFormat back from the data blob.
				Result = E_FAIL;
				XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);
			}
		}
		
		OutInfo.DeviceId = FString(DeviceId);
		OutInfo.NumChannels = FMath::Clamp((int32)WaveFormatEx->nChannels, 2, 8);
		OutInfo.SampleRate = WaveFormatEx->nSamplesPerSec;

		// XAudio2 automatically converts the audio format to output device us so we don't need to do any format conversions
		OutInfo.Format = EAudioMixerStreamDataFormat::Float;

		OutInfo.OutputChannelArray.Reset();

		// Extensible format supports surround sound so we need to parse the channel configuration to build our channel output array
		if (WaveFormatEx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			// Cast to the extensible format to get access to extensible data
			const WAVEFORMATEXTENSIBLE* WaveFormatExtensible = (WAVEFORMATEXTENSIBLE*)WaveFormatEx;

			// Loop through the extensible format channel flags in the standard order and build our output channel array
			// From https://msdn.microsoft.com/en-us/library/windows/hardware/dn653308(v=vs.85).aspx
			// The channels in the interleaved stream corresponding to these spatial positions must appear in the order specified above. This holds true even in the 
			// case of a non-contiguous subset of channels. For example, if a stream contains left, bass enhance and right, then channel 1 is left, channel 2 is right, 
			// and channel 3 is bass enhance. This enables the linkage of multi-channel streams to well-defined multi-speaker configurations.

			uint32 ChanCount = 0;
			for (uint32 ChannelTypeIndex = 0; ChannelTypeIndex < EAudioMixerChannel::ChannelTypeCount && ChanCount < (uint32)OutInfo.NumChannels; ++ChannelTypeIndex)
			{
				if (WaveFormatExtensible->dwChannelMask & ChannelTypeMap[ChannelTypeIndex])
				{
					OutInfo.OutputChannelArray.Add((EAudioMixerChannel::Type)ChannelTypeIndex);
					++ChanCount;
				}
			}

			// We didn't match channel masks for all channels, revert to a default ordering
			if (ChanCount < (uint32)OutInfo.NumChannels)
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("Did not find the channel type flags for audio device '%s'. Reverting to a default channel ordering."), *OutInfo.Name);

				OutInfo.OutputChannelArray.Reset();

				static EAudioMixerChannel::Type DefaultChannelOrdering[] = {
					EAudioMixerChannel::FrontLeft,
					EAudioMixerChannel::FrontRight,
					EAudioMixerChannel::FrontCenter,
					EAudioMixerChannel::LowFrequency,
					EAudioMixerChannel::SideLeft,
					EAudioMixerChannel::SideRight,
					EAudioMixerChannel::BackLeft,
					EAudioMixerChannel::BackRight,
				};

				EAudioMixerChannel::Type* ChannelOrdering = DefaultChannelOrdering;

				// Override channel ordering for some special cases
				if (OutInfo.NumChannels == 4)
				{
					static EAudioMixerChannel::Type DefaultChannelOrderingQuad[] = {
						EAudioMixerChannel::FrontLeft,
						EAudioMixerChannel::FrontRight,
						EAudioMixerChannel::BackLeft,
						EAudioMixerChannel::BackRight,
					};

					ChannelOrdering = DefaultChannelOrderingQuad;
				}
				else if (OutInfo.NumChannels == 6)
				{
					static EAudioMixerChannel::Type DefaultChannelOrdering51[] = {
						EAudioMixerChannel::FrontLeft,
						EAudioMixerChannel::FrontRight,
						EAudioMixerChannel::FrontCenter,
						EAudioMixerChannel::LowFrequency,
						EAudioMixerChannel::BackLeft,
						EAudioMixerChannel::BackRight,
					};

					ChannelOrdering = DefaultChannelOrdering51;
				}

				check(OutInfo.NumChannels <= 8);
				for (int32 Index = 0; Index < OutInfo.NumChannels; ++Index)
				{
					OutInfo.OutputChannelArray.Add(ChannelOrdering[Index]);
				}
			}
		}
		else
		{
			// Non-extensible formats only support mono or stereo channel output
			OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontLeft);
			if (OutInfo.NumChannels == 2)
			{
				OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontRight);
			}
		}

	Cleanup:
		PropVariantClear(&FriendlyName);
		PropVariantClear(&DeviceFormat);
		SAFE_RELEASE(PropertyStore);

		return SUCCEEDED(Result);
	}
#endif //  #if PLATFORM_WINDOWS

	bool FMixerPlatformXAudio2::GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo)
	{
		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_GetOutputDeviceInfo, FColor::Blue);
				
		// Use Cache if we have it. (index is a bad way to find the device, but we do it here).
		if (IAudioPlatformDeviceInfoCache* Cache = GetDeviceInfoCache())
		{
			if (InDeviceIndex == AUDIO_MIXER_DEFAULT_DEVICE_INDEX)
			{
				if (TOptional<FAudioPlatformDeviceInfo> Defaults = Cache->FindDefaultOutputDevice())
				{
					OutInfo = *Defaults;
					return true;
				}
			}
			else
			{
				TArray<FAudioPlatformDeviceInfo> ActiveDevices = Cache->GetAllActiveOutputDevices();
				if (ActiveDevices.IsValidIndex(InDeviceIndex))
				{
					OutInfo = ActiveDevices[InDeviceIndex];
					return true;
				}
			}
			return false;
		}
		
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("XAudio2 was not initialized."), Error);
			return false;
		}

#if PLATFORM_WINDOWS
		IMMDeviceEnumerator* DeviceEnumerator = nullptr;
		IMMDeviceCollection* DeviceCollection = nullptr;
		IMMDevice* DefaultDevice = nullptr;
		IMMDevice* Device = nullptr;
		bool bIsDefault = false;
		bool bSucceeded = false;

		HRESULT Result = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&DeviceEnumerator));
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		Result = DeviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &DeviceCollection);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		uint32 DeviceCount;
		Result = DeviceCollection->GetCount(&DeviceCount);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		if (DeviceCount == 0)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("No available audio device"));
			Result = S_FALSE;

			goto Cleanup;
		}

		// Get the default device
		Result = DeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &DefaultDevice);
		XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

		// If we are asking to get info on default device
		if (InDeviceIndex == AUDIO_MIXER_DEFAULT_DEVICE_INDEX)
		{
			Device = DefaultDevice;
			bIsDefault = true;
		}
		// Make sure we're not asking for a bad device index
		else if (InDeviceIndex >= DeviceCount)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Requested device index (%d) is larger than the number of devices available (%d)"), InDeviceIndex, DeviceCount);
			Result = S_FALSE;
			goto Cleanup;
		}
		else
		{
			Result = DeviceCollection->Item(InDeviceIndex, &Device);
			XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);
		}

		if (ensure(Device))
		{
			bSucceeded = GetMMDeviceInfo(Device, OutInfo);

			// Fix up if this was a default device
			if (bIsDefault)
			{
				OutInfo.bIsSystemDefault = true;
			}
			else if(DefaultDevice)
			{
				FAudioPlatformDeviceInfo DefaultInfo;
				GetMMDeviceInfo(DefaultDevice, DefaultInfo);
				OutInfo.bIsSystemDefault = OutInfo.DeviceId == DefaultInfo.DeviceId;
			}
		}

	Cleanup:
		SAFE_RELEASE(Device);
		SAFE_RELEASE(DeviceCollection);
		SAFE_RELEASE(DeviceEnumerator);

		return bSucceeded && SUCCEEDED(Result);

#else // #elif PLATFORM_WINDOWS
		OutInfo.bIsSystemDefault = true;
		OutInfo.SampleRate = 44100;
		OutInfo.DeviceId = 0;
		OutInfo.Format = EAudioMixerStreamDataFormat::Float;
		OutInfo.Name = TEXT("Audio Device.");
		OutInfo.NumChannels = 8;

		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontLeft);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontRight);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontCenter);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::LowFrequency);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::BackLeft);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::BackRight);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::SideLeft);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::SideRight);
		return true;
#endif 
	}

	bool FMixerPlatformXAudio2::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
	{
		OutDefaultDeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX;
		return true;
	}

	bool FMixerPlatformXAudio2::OpenAudioStream(const FAudioMixerOpenStreamParams& Params)
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("XAudio2 was not initialized."), Error);
			return false;
		}

		if (bIsDeviceOpen)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("XAudio2 audio stream already opened."), Warning);
			return false;
		}

		check(XAudio2System);
		check(OutputAudioStreamMasteringVoice == nullptr);

		WAVEFORMATEX Format = { 0 };

		OpenStreamParams = Params;

		AudioStreamInfo.Reset();

		AudioStreamInfo.OutputDeviceIndex = OpenStreamParams.OutputDeviceIndex;
		AudioStreamInfo.NumOutputFrames = OpenStreamParams.NumFrames;
		AudioStreamInfo.NumBuffers = OpenStreamParams.NumBuffers;
		AudioStreamInfo.AudioMixer = OpenStreamParams.AudioMixer;

		uint32 NumOutputDevices = 0;
		HRESULT Result = ERROR_SUCCESS;

		if (GetNumOutputDevices(NumOutputDevices) && NumOutputDevices > 0)
		{
			if (!GetOutputDeviceInfo(AudioStreamInfo.OutputDeviceIndex, AudioStreamInfo.DeviceInfo))
			{
				return false;
			}

			// Store the device ID here in case it is removed. We can switch back if the device comes back.
			if (Params.bRestoreIfRemoved)
			{
				OriginalAudioDeviceId = AudioStreamInfo.DeviceInfo.DeviceId;
			}

#if PLATFORM_WINDOWS			
			Result = XAudio2System->CreateMasteringVoice(
				&OutputAudioStreamMasteringVoice,
				AudioStreamInfo.DeviceInfo.NumChannels,
				AudioStreamInfo.DeviceInfo.SampleRate,
				0,
				*AudioStreamInfo.DeviceInfo.DeviceId,
				nullptr,
				AudioCategory_GameEffects);
#else //PLATFORM_WINDOWS			
			Result = XAudio2System->CreateMasteringVoice(
				&OutputAudioStreamMasteringVoice,
				AudioStreamInfo.DeviceInfo.NumChannels,
				AudioStreamInfo.DeviceInfo.SampleRate,
				0,
				nullptr,
				nullptr);
#endif //#else PLATFORM_WINDOWS

			XAUDIO2_GOTO_CLEANUP_ON_FAIL(Result);

			// Start the xaudio2 engine running, which will now allow us to start feeding audio to it
			XAudio2System->StartEngine();

			// Setup the format of the output source voice
			Format.nChannels = AudioStreamInfo.DeviceInfo.NumChannels;
			Format.nSamplesPerSec = Params.SampleRate;
			Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			Format.nAvgBytesPerSec = Format.nSamplesPerSec * sizeof(float) * Format.nChannels;
			Format.nBlockAlign = sizeof(float) * Format.nChannels;
			Format.wBitsPerSample = sizeof(float) * 8;

			// Create the output source voice
			Result = XAudio2System->CreateSourceVoice(&OutputAudioStreamSourceVoice, &Format, XAUDIO2_VOICE_NOPITCH, 2.0f, &OutputVoiceCallback);
			XAUDIO2_RETURN_ON_FAIL(Result);
		}
	Cleanup:

		bool bXAudioOpenSuccessfully = OutputAudioStreamSourceVoice && OutputAudioStreamMasteringVoice;
		if (!bXAudioOpenSuccessfully)
		{
			// Undo anything we created.
			if (OutputAudioStreamSourceVoice)
			{
				OutputAudioStreamSourceVoice->DestroyVoice();
				OutputAudioStreamSourceVoice = nullptr;
			}		
			if (OutputAudioStreamMasteringVoice)
			{
				OutputAudioStreamMasteringVoice->DestroyVoice();
				OutputAudioStreamMasteringVoice = nullptr;
			}

			// Setup for running null device.
			AudioStreamInfo.NumOutputFrames = OpenStreamParams.NumFrames;
			AudioStreamInfo.DeviceInfo.OutputChannelArray = { EAudioMixerChannel::FrontLeft, EAudioMixerChannel::FrontRight };
			AudioStreamInfo.DeviceInfo.NumChannels = 2;
			AudioStreamInfo.DeviceInfo.SampleRate = OpenStreamParams.SampleRate;
			AudioStreamInfo.DeviceInfo.Format = EAudioMixerStreamDataFormat::Float;
		}

#if PLATFORM_WINDOWS
		if(!bXAudioOpenSuccessfully)
		{
			// On Windows where we can have audio devices unplugged/removed/hot-swapped:
			// We must mark ourselves open, even if we failed to open. This will allow the device-swap logic to run.
			// StartAudioStream will happily use the null renderer path if there's no real stream open.
			bXAudioOpenSuccessfully = true;
		}
#endif //PLATFORM_WINDOWS		

		// If we opened, mark the stream as open.
		if(bXAudioOpenSuccessfully)
		{
			AudioStreamInfo.StreamState = EAudioOutputStreamState::Open;
			bIsDeviceOpen = true;
		}	

		return bXAudioOpenSuccessfully;
	}

	FAudioPlatformDeviceInfo FMixerPlatformXAudio2::GetPlatformDeviceInfo() const
	{
		return AudioStreamInfo.DeviceInfo;
	}

	FString FMixerPlatformXAudio2::GetCurrentDeviceName() const
	{
		return AudioStreamInfo.DeviceInfo.Name;
	}

	bool FMixerPlatformXAudio2::CloseAudioStream()
	{
		if (!bIsInitialized || AudioStreamInfo.StreamState == EAudioOutputStreamState::Closed)
		{
			return false;
		}

		// If we have a active device swap in flight, we need to wait for it.
		static const FTimespan TimeoutOnDeviceSwap(0,0,0,3,0); // 3 secs.
		if (ActiveDeviceSwap.IsValid() && !ActiveDeviceSwap.IsReady() && !ActiveDeviceSwap.WaitFor(TimeoutOnDeviceSwap))
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Timeout waiting for inflight device-swap. InstanceID=%d"), InstanceID);
			return false;
		}

		if (bIsDeviceOpen && !StopAudioStream())
		{
			return false;
		}

		if (XAudio2System)
		{
			XAudio2System->StopEngine();
		}

		if (OutputAudioStreamSourceVoice)
		{
			OutputAudioStreamSourceVoice->DestroyVoice();
			OutputAudioStreamSourceVoice = nullptr;
		}

		check(OutputAudioStreamMasteringVoice || bIsUsingNullDevice);
		if (OutputAudioStreamMasteringVoice)
		{
			OutputAudioStreamMasteringVoice->DestroyVoice();
			OutputAudioStreamMasteringVoice = nullptr;
		}
		else
		{
			StopRunningNullDevice();
		}

		bIsDeviceOpen = false;

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;

		return true;
	}

	bool FMixerPlatformXAudio2::StartAudioStream()
	{
		UE_LOG(LogAudioMixer, Log, TEXT("FMixerPlatformXAudio2::StartAudioStream() called. InstanceID=%d"), InstanceID);
		// Start generating audio with our output source voice
		BeginGeneratingAudio();

		// If we already have a source voice, we can just restart it
		if (OutputAudioStreamSourceVoice)
		{
			AudioStreamInfo.StreamState = EAudioOutputStreamState::Running;
			OutputAudioStreamSourceVoice->Start();
		}
		else
		{
			check(!bIsUsingNullDevice);
			StartRunningNullDevice();
		}

		return true;
	}

	bool FMixerPlatformXAudio2::StopAudioStream()
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("XAudio2 was not initialized."), Warning);
			return false;
		}

		UE_LOG(LogAudioMixer, Log, TEXT("FMixerPlatformXAudio2::StopAudioStream() called. InstanceID=%d"), InstanceID);

		if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped && AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
		{
			if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running)
			{
				StopGeneratingAudio();
			}

			// Signal that the thread that is running the update that we're stopping
			if (OutputAudioStreamSourceVoice)
			{
				OutputAudioStreamSourceVoice->Stop(0, 0); // Don't wait for tails, stop as quick as you can.

			}

			check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
		}

		return true;
	}

	bool FMixerPlatformXAudio2::CheckAudioDeviceChange()
	{
#if PLATFORM_WINDOWS && XAUDIO_SUPPORTS_DEVICE_DETAILS

		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_CheckAudioDeviceChange, FColor::Blue);

		// Use threaded version? (It also requires the info cache).
		if (IAudioMixer::ShouldUseThreadedDeviceSwap() && IAudioMixer::ShouldUseDeviceInfoCache())
		{
			return CheckThreadedDeviceSwap();
		}

		FScopeLock Lock(&AudioDeviceSwapCriticalSection);
		if (bMoveAudioStreamToNewAudioDevice)
		{
			bMoveAudioStreamToNewAudioDevice = false;

			return MoveAudioStreamToNewAudioDevice(NewAudioDeviceId);
		}
#endif
		return false;
	}

	bool FMixerPlatformXAudio2::RequestDeviceSwap(const FString& DeviceID, bool bInForce, const TCHAR* InReason)
	{
		if (!AllowDeviceSwap() && !bInForce)
		{
			UE_LOG(LogAudioMixer, Verbose, TEXT("NOT-ALLOWING attempt to swap audio render device to new device: '%s', because: '%s', force=%d, InstanceID=%d"),
				!DeviceID.IsEmpty() ? *DeviceID : TEXT("[System Default]"),
				InReason ? InReason : TEXT("None specified"),
				(int32)bInForce,
				InstanceID
			);
			return false;
		}

		if (AudioDeviceSwapCriticalSection.TryLock())
		{
			UE_LOG(LogAudioMixer, Verbose, TEXT("Attempt to swap audio render device to new device: '%s', because: '%s', force=%d, InstanceID=%d"),
				!DeviceID.IsEmpty() ? *DeviceID : TEXT("[System Default]"),
				InReason ? InReason : TEXT("None specified"),
				(int32)bInForce,
				InstanceID
			);

			NewAudioDeviceId = DeviceID;
			bMoveAudioStreamToNewAudioDevice = true;
			DeviceSwapReason = InReason;
			
			AudioDeviceSwapCriticalSection.Unlock();

			return true;
		}

		UE_CLOG(bInForce,LogAudioMixer, Warning, TEXT("FMixerPlatformXAudio2::RequestDeviceSwap(), Failed to acquire CS, device swap to '%s', will be ignored. InstanceID=%d"),
			!DeviceID.IsEmpty() ? *DeviceID : TEXT("[System Default]"), InstanceID );
		return false;
	}
		
	bool FMixerPlatformXAudio2::MoveAudioStreamToNewAudioDevice(const FString& InNewDeviceId)
	{
		bool bDidStopGeneratingAudio = false;

#if PLATFORM_WINDOWS && XAUDIO_SUPPORTS_DEVICE_DETAILS

		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_MoveAudioStreamToNewAudioDevice, FColor::Blue);

		uint32 NumDevices = 0;
		// XAudio2 for HoloLens doesn't have GetDeviceCount, use local wrapper instead
		if (!GetNumOutputDevices(NumDevices))
		{
			return bDidStopGeneratingAudio;
		}

		// If we're running the null device, This function is called every second or so.
		// Because of this, we early exit from this function if we're running the null device
		// and there still are no devices.
		const bool bTrySwitchToHardwareDevice = NumDevices > 0;
		const bool bContinueUsingNullDevice = bIsUsingNullDevice && !bTrySwitchToHardwareDevice;
		const bool bSwitchToNullDevice = !bIsUsingNullDevice && !bTrySwitchToHardwareDevice;

		if (bContinueUsingNullDevice)
		{
			// Audio device was not changed. Return false to avoid downstream device change logic.
			return bDidStopGeneratingAudio;
		}

		UE_LOG(LogAudioMixer, Log, TEXT("Resetting audio stream to device id '%s'"), 
			!InNewDeviceId.IsEmpty() ? *InNewDeviceId : TEXT("[System Default]") );

		// Device swaps require reinitialization of output buffers to handle
		// different channel formats. Stop generating audio to protect against
		// accessing the OutputBuffer.
		StopGeneratingAudio();
		bDidStopGeneratingAudio = true;

		// Stop currently running device
		if (bIsUsingNullDevice)
		{
			StopRunningNullDevice();
		}
		else
		{
			// Not initialized!
			if (!bIsInitialized)
			{
				return bDidStopGeneratingAudio;
			}

			SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_MoveAudioStreamToNewAudioDevice_DestroyVoices, FColor::Blue);

			// If an XAudio2 callback is in flight,
			// we have to wait for it here.
			FScopeLock ScopeLock(&DeviceSwapCriticalSection);

			// Now that we've properly locked, raise the bIsInDeviceSwap flag
			// in case FlushSourceBuffers() calls OnBufferEnd on this thread,
			// and DeviceSwapCriticalSection.TryLock() is still returning true
			bIsInDeviceSwap = true;

			// Flush all buffers. Because we've locked DeviceSwapCriticalSection, ReadNextBuffer will early exit and we will not submit any additional buffers.
			if (OutputAudioStreamSourceVoice)
			{
				OutputAudioStreamSourceVoice->FlushSourceBuffers();
			}

			if (OutputAudioStreamSourceVoice)
			{
				// Then destroy the current audio stream source voice
				OutputAudioStreamSourceVoice->DestroyVoice();
				OutputAudioStreamSourceVoice = nullptr;
			}

			// Now destroy the mastering voice
			if (OutputAudioStreamMasteringVoice)
			{
				OutputAudioStreamMasteringVoice->DestroyVoice();
				OutputAudioStreamMasteringVoice = nullptr;
			}

			bIsInDeviceSwap = false;
		}

		// In order to resume audio playback, this function must return true. 
		// All code paths below return true, even it they encounter an error.
		
		if (bTrySwitchToHardwareDevice)
		{
			if (!ResetXAudio2System())
			{
				// Reinitializing the XAudio2System failed, so we have to exit here.
				return bDidStopGeneratingAudio;
			}

			// Now get info on the new audio device we're trying to reset to
			uint32 DeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX;
			if (!InNewDeviceId.IsEmpty())
			{
				// XAudio2 for HoloLens doesn't have GetDeviceDetails, use local wrapper instead
				FAudioPlatformDeviceInfo DeviceDetails;
				for (uint32 i = 0; i < NumDevices; ++i)
				{
					GetOutputDeviceInfo(i, DeviceDetails);
					if (DeviceDetails.DeviceId == InNewDeviceId)
					{
						DeviceIndex = i;
						break;
					}
				}
			}

			// Update the audio stream info to the new device info
			AudioStreamInfo.OutputDeviceIndex = DeviceIndex;

			// Get the output device info at this new index
			GetOutputDeviceInfo(AudioStreamInfo.OutputDeviceIndex, AudioStreamInfo.DeviceInfo);

			HRESULT Result;
			// Create a new master voice
			{
				SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_MoveAudioStreamToNewAudioDevice_CreateMaster, FColor::Blue);

				// open up on the default device
				Result = XAudio2System->CreateMasteringVoice(
					&OutputAudioStreamMasteringVoice,
					AudioStreamInfo.DeviceInfo.NumChannels,
					AudioStreamInfo.DeviceInfo.SampleRate,
					0,
					*AudioStreamInfo.DeviceInfo.DeviceId,
					nullptr,
					AudioCategory_GameEffects);
				if (FAILED(Result))
				{
					XAUDIO2_LOG_RESULT("XAudio2System->CreateMasteringVoice", Result);
					// Switch to running null device by setting OutputAudioStreamMasteringVoice to null.
					// Will default to null device when calling `ResumePlaybackOnNewDevice()`
					OutputAudioStreamMasteringVoice = nullptr;
				}			
				UE_CLOG(SUCCEEDED(Result), LogAudioMixer, Display, TEXT("XAudio2 CreateMasterVoice Channels=%u SampleRate=%u DeviceId=%s Name=%s"),
					AudioStreamInfo.DeviceInfo.NumChannels, AudioStreamInfo.DeviceInfo.SampleRate, *AudioStreamInfo.DeviceInfo.DeviceId, *AudioStreamInfo.DeviceInfo.Name
				);
			}

			// Setup the format of the output source voice
			WAVEFORMATEX Format = { 0 };
			Format.nChannels = AudioStreamInfo.DeviceInfo.NumChannels;
			Format.nSamplesPerSec = OpenStreamParams.SampleRate;
			Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			Format.nAvgBytesPerSec = Format.nSamplesPerSec * sizeof(float) * Format.nChannels;
			Format.nBlockAlign = sizeof(float) * Format.nChannels;
			Format.wBitsPerSample = sizeof(float) * 8;

			{				
				SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_MoveAudioStreamToNewAudioDevice_CreateSource, FColor::Blue);

				// Create the output source voice
				Result = XAudio2System->CreateSourceVoice(&OutputAudioStreamSourceVoice, &Format, XAUDIO2_VOICE_NOPITCH, 2.0f, &OutputVoiceCallback);
				if (FAILED(Result))
				{
					XAUDIO2_LOG_RESULT("XAudio2System->CreateSourceVoice", Result);
					// Switch to running null device by setting OutputAudioStreamSourceVoice to null.
					// Will default to null device when calling `ResumePlaybackOnNewDevice()`
					OutputAudioStreamSourceVoice = nullptr;
				}
			}

			// Reinitialize the output circular buffer to match the buffer math of the new audio device.
			const int32 NumOutputSamples = AudioStreamInfo.NumOutputFrames * AudioStreamInfo.DeviceInfo.NumChannels;
			if (ensure(NumOutputSamples > 0))
			{
				SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_MoveAudioStreamToNewAudioDevice_OutBufferInit, FColor::Blue);
				OutputBuffer.Init(AudioStreamInfo.AudioMixer, NumOutputSamples, NumOutputBuffers, AudioStreamInfo.DeviceInfo.Format);
			}
		}
		else
		{	
			check(bSwitchToNullDevice);

			// If we don't have any hardware playback devices available, use the null device callback to render buffers.
			// NullDevice is started when OutputAudioStreamSourceVoice is null
			check(nullptr == OutputAudioStreamSourceVoice);

			return bDidStopGeneratingAudio;
		}
		
#endif // #if PLATFORM_WINDOWS

		return bDidStopGeneratingAudio; 
	}

	void FMixerPlatformXAudio2::ResumePlaybackOnNewDevice()
	{
		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_ResumePlaybackOnNewDevice, FColor::Blue);
		
		int32 NumSamplesPopped = 0;
		TArrayView<const uint8> PoppedAudio = OutputBuffer.PopBufferData(NumSamplesPopped);
		SubmitBuffer(PoppedAudio.GetData());

		check(OpenStreamParams.NumFrames * AudioStreamInfo.DeviceInfo.NumChannels == OutputBuffer.GetNumSamples());

		if (nullptr == AudioRenderEvent)
		{
			BeginGeneratingAudio();
		}

		AudioRenderEvent->Trigger();

		if (OutputAudioStreamSourceVoice)
		{
			// Start the voice streaming
			OutputAudioStreamSourceVoice->Start();
		}
		else
		{
			StartRunningNullDevice();
		}
	}

	void FMixerPlatformXAudio2::SubmitBuffer(const uint8* Buffer)
	{
		SCOPED_NAMED_EVENT(FMixerPlatformXAudio2_SubmitBuffer, FColor::Blue);

		if (OutputAudioStreamSourceVoice)
		{
			// Create a new xaudio2 buffer submission
			XAUDIO2_BUFFER XAudio2Buffer = { 0 };
			XAudio2Buffer.AudioBytes = OpenStreamParams.NumFrames * AudioStreamInfo.DeviceInfo.NumChannels * sizeof(float);
			XAudio2Buffer.pAudioData = (const BYTE*)Buffer;
			XAudio2Buffer.pContext = this;

			// Submit buffer to the output streaming voice
			OutputAudioStreamSourceVoice->SubmitSourceBuffer(&XAudio2Buffer);

			if(!FirstBufferSubmitted)
			{
				UE_LOG(LogAudioMixer, Display, TEXT("FMixerPlatformXAudio2::SubmitBuffer() called for the first time. InstanceID=%d"), InstanceID);
				FirstBufferSubmitted = true;
			}
		}
	}

	FString FMixerPlatformXAudio2::GetDefaultDeviceName()
	{
		return FString();
	}

	FAudioPlatformSettings FMixerPlatformXAudio2::GetPlatformSettings() const
	{
#if WITH_ENGINE
		return FAudioPlatformSettings::GetPlatformSettings(FPlatformProperties::GetRuntimeSettingsClassName());
#else
		return FAudioPlatformSettings();
#endif // WITH_ENGINE
	}

	void FMixerPlatformXAudio2::OnHardwareUpdate()
	{
#if WITH_XMA2
		XMA2_INFO_CALL(FXMAAudioInfo::Tick());
#endif //WITH_XMA2

	}

	Audio::IAudioPlatformDeviceInfoCache* FMixerPlatformXAudio2::GetDeviceInfoCache() const
	{
		if (IAudioMixer::ShouldUseDeviceInfoCache())
		{
			return DeviceInfoCache.Get();
		}
		// Disabled.
		return nullptr;
	}

	void FMixerPlatformXAudio2::OnSessionDisconnect(IAudioMixerDeviceChangedListener::EDisconnectReason InReason)
	{
		// Device has disconnected from current session.
		if (InReason == IAudioMixerDeviceChangedListener::EDisconnectReason::FormatChanged)
		{
			// OnFormatChanged, retry again same device.
			RequestDeviceSwap(GetDeviceId(), /*force*/ true, TEXT("FMixerPlatformXAudio2::OnSessionDisconnect() - FormatChanged"));		
		}
		else if (InReason == IAudioMixerDeviceChangedListener::EDisconnectReason::DeviceRemoval)
		{
			// Ignore Device Removal, as this is handle by the Device Removal logic in the Notification Client.
		}
		else
		{
			// ServerShutdown, SessionLogoff, SessionDisconnected, ExclusiveModeOverride
			// Attempt a default swap, will likely fail, but then we'll switch to a null device.
			RequestDeviceSwap(TEXT(""), /*force*/ true, TEXT("FMixerPlatformXAudio2::OnSessionDisconnect() - Other"));
		}
	}

	bool FMixerPlatformXAudio2::DisablePCMAudioCaching() const
	{
		return true;
	}
}
