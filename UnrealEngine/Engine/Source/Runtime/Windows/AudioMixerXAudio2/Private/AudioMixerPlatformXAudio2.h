// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#if PLATFORM_WINDOWS
#include <xaudio2redist.h>
#else
#include <xaudio2.h>
#endif
#include "Windows/HideWindowsPlatformTypes.h"

#include "Async/Future.h"

#if PLATFORM_WINDOWS
#pragma comment(lib,"xaudio2_9redist.lib")
#endif

#ifndef XAUDIO_SUPPORTS_DEVICE_DETAILS
    #define XAUDIO_SUPPORTS_DEVICE_DETAILS		1
#endif	//XAUDIO_SUPPORTS_DEVICE_DETAILS

#ifndef PLATFORM_NEEDS_SUSPEND_ON_BACKGROUND
	#define PLATFORM_NEEDS_SUSPEND_ON_BACKGROUND	0
#endif //PLATFORM_NEEDS_SUSPEND_ON_BACKGROUND

// Any platform defines
namespace Audio
{
	class FMixerPlatformXAudio2;

	/**
	* FXAudio2VoiceCallback
	* XAudio2 implementation of IXAudio2VoiceCallback
	* This callback class is used to get event notifications on buffer end (when a buffer has finished processing).
	* This is used to signal the I/O thread that it can request another buffer from the user callback.
	*/
	class FXAudio2VoiceCallback final : public IXAudio2VoiceCallback
	{
	public:
		FXAudio2VoiceCallback() {}
		~FXAudio2VoiceCallback() {}

	private:
		void STDCALL OnVoiceProcessingPassStart(UINT32 BytesRequired) {}
		void STDCALL OnVoiceProcessingPassEnd() {}
		void STDCALL OnStreamEnd() {}
		void STDCALL OnBufferStart(void* BufferContext) {}
		void STDCALL OnLoopEnd(void* BufferContext) {}
		void STDCALL OnVoiceError(void* BufferContext, HRESULT Error) {}

		void STDCALL OnBufferEnd(void* BufferContext);

	};

	class FMixerPlatformXAudio2 : public IAudioMixerPlatformInterface
	{

	public:

		FMixerPlatformXAudio2();
		~FMixerPlatformXAudio2();

		//~ Begin IAudioMixerPlatformInterface
		virtual FString GetPlatformApi() const override { return TEXT("XAudio2"); }
		virtual bool InitializeHardware() override;
		virtual bool CheckAudioDeviceChange() override;
		virtual bool TeardownHardware() override;
		virtual bool IsInitialized() const override;
		virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) override;
		virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) override;
		virtual FString GetCurrentDeviceName() const override;
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override;
		virtual bool CloseAudioStream() override;
		virtual bool StartAudioStream() override;
		virtual bool StopAudioStream() override;
		virtual bool MoveAudioStreamToNewAudioDevice(const FString& InNewDeviceId) override;
		virtual bool RequestDeviceSwap(const FString& DeviceID, bool bInForce, const TCHAR* InReason) override;
		virtual void ResumePlaybackOnNewDevice() override;
		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override;
		virtual void SubmitBuffer(const uint8* Buffer) override;
		virtual bool DisablePCMAudioCaching() const override;
		virtual FString GetDefaultDeviceName() override;
		virtual FAudioPlatformSettings GetPlatformSettings() const override;
		virtual void OnHardwareUpdate() override;
		virtual IAudioPlatformDeviceInfoCache* GetDeviceInfoCache() const override;
		//~ End IAudioMixerPlatformInterface

		//~ Begin IAudioMixerDeviceChangedListener
		virtual void RegisterDeviceChangedListener() override;
		virtual void UnregisterDeviceChangedListener() override;
		virtual void OnDefaultCaptureDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;
		virtual void OnDefaultRenderDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;
		virtual void OnDeviceAdded(const FString& DeviceId, bool bIsRenderDevice) override;
		virtual void OnDeviceRemoved(const FString& DeviceId, bool bIsRenderDevice) override;
		virtual void OnDeviceStateChanged(const FString& DeviceId, const EAudioDeviceState InState, bool bIsRenderDevice) override;
		virtual void OnSessionDisconnect(Audio::IAudioMixerDeviceChangedListener::EDisconnectReason InReason) override;
		virtual FString GetDeviceId() const override;		
		//~ End IAudioMixerDeviceChangedListener

	private:
	
		struct FXAudio2AsyncCreateResult
		{
			IXAudio2* XAudio2System = nullptr;
			IXAudio2MasteringVoice* OutputAudioStreamMasteringVoice = nullptr;
			IXAudio2SourceVoice* OutputAudioStreamSourceVoice = nullptr;
			FAudioPlatformDeviceInfo DeviceInfo;
			FString SwapReason;
			float SuccessfullDurationMs = 0.f;
		};
		TFuture<FXAudio2AsyncCreateResult> ActiveDeviceSwap;
		bool CheckThreadedDeviceSwap();
	
		bool AllowDeviceSwap();

		// Used to teardown and reinitialize XAudio2.
		// This must be done to repopulate the playback device list in XAudio 2.7.
		bool ResetXAudio2System();

		void Suspend();
		void Resume();

		// Handle to XAudio2DLL
		FName DllName;
		HMODULE XAudio2Dll;

		FDelegateHandle DeactiveHandle;
		FDelegateHandle ReactivateHandle;
		FDelegateHandle EnteredBackgroundHandle;
		FDelegateHandle EnteredForegroundHandle;

		// Bool indicating that the default audio device changed
		// And that we need to restart the audio device.
		FThreadSafeBool bDeviceChanged;

		IXAudio2* XAudio2System;
		IXAudio2MasteringVoice* OutputAudioStreamMasteringVoice;
		IXAudio2SourceVoice* OutputAudioStreamSourceVoice;
		FXAudio2VoiceCallback OutputVoiceCallback;
		FCriticalSection AudioDeviceSwapCriticalSection;
		FString OriginalAudioDeviceId;
		FString NewAudioDeviceId;
		double LastDeviceSwapTime;
		FString DeviceSwapReason;

		// When we are running the null device,
		// we check whether a new audio device was connected every second or so.
		float TimeSinceNullDeviceWasLastChecked;

		bool FirstBufferSubmitted{false};

		TUniquePtr<IAudioPlatformDeviceInfoCache> DeviceInfoCache;

		uint32 bIsInitialized : 1;
		uint32 bIsDeviceOpen : 1;
		uint32 bIsSuspended : 1;
	};
}

