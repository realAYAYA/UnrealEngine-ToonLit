// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreSubsystem.h"
#include "AudioDeviceManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaIOCoreSubsystem)

void UMediaIOCoreSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	DeviceDestroyedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddUObject(this, &UMediaIOCoreSubsystem::OnAudioDeviceDestroyed);
}

void UMediaIOCoreSubsystem::Deinitialize()
{
	FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(DeviceDestroyedHandle);
}

TSharedPtr<FMediaIOAudioOutput> UMediaIOCoreSubsystem::CreateAudioOutput(const FCreateAudioOutputArgs& InArgs)
{
	TSharedPtr<FMediaIOAudioCapture> MediaIOAudioCapture;

	if (InArgs.AudioDeviceHandle.IsValid())
	{
		if (const TSharedPtr<FMediaIOAudioCapture, ESPMode::ThreadSafe>* FoundMediaIOAudioCapture = MediaIOAudioCaptures.Find(InArgs.AudioDeviceHandle.GetDeviceID()))
		{
			MediaIOAudioCapture = *FoundMediaIOAudioCapture;
		}
		else
		{
			MediaIOAudioCapture = MediaIOAudioCaptures.Add(InArgs.AudioDeviceHandle.GetDeviceID(), MakeShared<FMediaIOAudioCapture>());
			MediaIOAudioCapture->Initialize(InArgs.AudioDeviceHandle);
			MainMediaIOAudioCapture->OnAudioCaptured_RenderThread().BindUObject(this, &UMediaIOCoreSubsystem::OnBufferReceivedByCapture, InArgs.AudioDeviceHandle.GetDeviceID());
		}
	}
	else
	{
		// Fallback using the main audio device.
		if (!MainMediaIOAudioCapture)
		{
			MainMediaIOAudioCapture = MakeShared<FMainMediaIOAudioCapture>();
			MainMediaIOAudioCapture->Initialize();
			MainMediaIOAudioCapture->OnAudioCaptured_RenderThread().BindUObject(this, &UMediaIOCoreSubsystem::OnBufferReceivedByCapture, InArgs.AudioDeviceHandle.GetDeviceID());
		}
		MediaIOAudioCapture = MainMediaIOAudioCapture;
	}	

	return MediaIOAudioCapture->CreateAudioOutput(InArgs.NumOutputChannels, InArgs.TargetFrameRate, InArgs.MaxSampleLatency, InArgs.OutputSampleRate);
}

int32 UMediaIOCoreSubsystem::GetNumAudioInputChannels() const
{
	if (MainMediaIOAudioCapture)
	{
		return MainMediaIOAudioCapture->GetNumInputChannels();
	}
	return 0;
}

void UMediaIOCoreSubsystem::OnAudioDeviceDestroyed(Audio::FDeviceId InAudioDeviceId)
{
	if (MediaIOAudioCaptures.Contains(InAudioDeviceId))
	{
		MediaIOAudioCaptures.Remove(InAudioDeviceId);
	}
}

void UMediaIOCoreSubsystem::OnBufferReceivedByCapture(float* Data, int32 NumSamples, Audio::FDeviceId AudioDeviceID) const
{
	BufferReceivedDelegate.Broadcast(AudioDeviceID, Data, NumSamples);
}
