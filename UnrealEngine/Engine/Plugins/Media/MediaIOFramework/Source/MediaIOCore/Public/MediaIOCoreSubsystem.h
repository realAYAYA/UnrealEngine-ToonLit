// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDeviceHandle.h"

#include "AudioDeviceHandle.h"
#include "MediaIOCoreAudioOutput.h"
#include "Subsystems/EngineSubsystem.h"

#include "MediaIOCoreSubsystem.generated.h"

UCLASS()
class MEDIAIOCORE_API UMediaIOCoreSubsystem : public UEngineSubsystem
{
public:
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnBufferReceived, Audio::FDeviceId /* DeviceId */, float* /* Data */, int32 /* NumSamples */)
	
	struct FCreateAudioOutputArgs
	{
		uint32 NumOutputChannels = 0;
		FFrameRate TargetFrameRate; 
		uint32 MaxSampleLatency = 0;
		uint32 OutputSampleRate = 0;
		FAudioDeviceHandle AudioDeviceHandle;
	};

public:
	GENERATED_BODY()

	//~ Begin UEngineSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
	virtual void Deinitialize() override;
	//~ End UEngineSubsystem Interface

	/**
	 * Create an audio output that allows getting audio that was accumulated during the last frame. 
	 */
	TSharedPtr<FMediaIOAudioOutput> CreateAudioOutput(const FCreateAudioOutputArgs& InArgs);

	/**
	 * Get the number of audio channels used by the main audio device.
	 **/
	int32 GetNumAudioInputChannels() const;

	/**
	 * @Note: Called from the audio thread.
	 */
	FOnBufferReceived& OnBufferReceived_AudioThread()
	{
		return BufferReceivedDelegate;
	}

private:
	void OnAudioDeviceDestroyed(Audio::FDeviceId InAudioDeviceId);
	void OnBufferReceivedByCapture(float* Data, int32 NumSamples, Audio::FDeviceId AudioDeviceID) const;

private:
	TSharedPtr<FMainMediaIOAudioCapture> MainMediaIOAudioCapture;
	
	TMap<Audio::FDeviceId, TSharedPtr<FMediaIOAudioCapture, ESPMode::ThreadSafe>> MediaIOAudioCaptures;

	FDelegateHandle DeviceDestroyedHandle;

	FOnBufferReceived BufferReceivedDelegate;
};
