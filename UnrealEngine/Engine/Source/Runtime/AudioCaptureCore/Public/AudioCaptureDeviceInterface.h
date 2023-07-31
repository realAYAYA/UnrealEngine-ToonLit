// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"


namespace Audio
{
	enum class EHardwareInputFeature : uint8;
	
	struct FCaptureDeviceInfo
 	{
		FString DeviceName;
		FString DeviceId;
		int32 InputChannels;
		int32 PreferredSampleRate;
		bool bSupportsHardwareAEC;
	};

	const int32 DefaultDeviceIndex = INDEX_NONE;

	struct FAudioCaptureDeviceParams
	{
		// set to true to use this device's built-in echo cancellation, if possible.
		bool bUseHardwareAEC;
		// Set this to INDEX_NONE 
		int32 DeviceIndex;

		FAudioCaptureDeviceParams()
			: bUseHardwareAEC(false)
			, DeviceIndex(DefaultDeviceIndex)
		{
		}
	};

	typedef TFunction<void(const float* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)> FOnCaptureFunction;
	
	class IAudioCaptureStream : public IModularFeature
	{
	public:
		IAudioCaptureStream() {}
		virtual ~IAudioCaptureStream() {}

		
		// Lets us know which users are in the system.
		virtual bool RegisterUser(const TCHAR* UserId) { return true; }
		// Call this to remove a user that was added with RegisterUser.
		virtual bool UnregisterUser(const TCHAR* UserId) { return true; }
		// Returns the audio capture device information at the given Id.
		virtual bool GetCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo, int32 DeviceIndex) = 0;
		// Opens the audio capture stream with the given parameters
		virtual bool OpenCaptureStream(const FAudioCaptureDeviceParams& InParams, FOnCaptureFunction InOnCapture, uint32 NumFramesDesired) = 0;
		// Closes the audio capture stream
		virtual bool CloseStream() = 0;
		// Start the audio capture stream
		virtual bool StartStream() = 0;
		// Stop the audio capture stream
		virtual bool StopStream() = 0;
		// Abort the audio capture stream (stop and close)
		virtual bool AbortStream() = 0;
		// Get the stream time of the audio capture stream
		virtual bool GetStreamTime(double& OutStreamTime) = 0;
		// Get the sample rate in use by the stream.
		virtual int32 GetSampleRate() const = 0;
		// Returns if the audio capture stream has been opened.
		virtual bool IsStreamOpen() const = 0;
		// Returns true if the audio capture stream is currently capturing audio.
		virtual bool IsCapturing() const = 0;

		// This is the callback for querying audio from the input device.
		virtual void OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow) = 0;

		// Returns the total amount of audio devices.
		virtual bool GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices) = 0;

		virtual bool GetIfHardwareFeatureIsSupported(EHardwareInputFeature FeatureType) 
		{ 
			return false;
		}

		virtual void SetHardwareFeatureEnabled(EHardwareInputFeature FeatureType, bool bEnabled)
		{
		}
	};

	class IAudioCaptureFactory : public IModularFeature
	{
	public:
		IAudioCaptureFactory() {}
		virtual ~IAudioCaptureFactory() {}

		// IModularFeature name. Use this to register an implementation of IAudioCaptureDevice.
		static FName GetModularFeatureName()
		{
			static FName AudioCaptureFeatureName = FName(TEXT("AudioCaptureStream"));
			return AudioCaptureFeatureName;
		}

		virtual TUniquePtr<IAudioCaptureStream> CreateNewAudioCaptureStream() = 0;
	};

} // namespace Audio