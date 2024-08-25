// Copyright Epic Games, Inc. All Rights Reserved.

#include "BufferedSubmixListener.h"
#include "AudioMixerDevice.h"

namespace BufferedSubmixListenerPrivate
{
	static Audio::FDeviceId InvalidAudioDeviceId = static_cast<Audio::FDeviceId>(INDEX_NONE);
}

/** Buffered Submix Listener. */
FBufferedSubmixListener::FBufferedSubmixListener(int32 InDefaultCircularBufferSize, bool bInZeroInputBuffer, const FString* InName)
	: FBufferedListenerBase{ InDefaultCircularBufferSize }
	, DeviceId{ BufferedSubmixListenerPrivate::InvalidAudioDeviceId }
	, bZeroInputBuffer{ bInZeroInputBuffer }
{
	if (InName)
	{
		Name = *InName;
	}
}

FBufferedSubmixListener::~FBufferedSubmixListener()
{
	// We should have been unregistered.
	check(DeviceId == BufferedSubmixListenerPrivate::InvalidAudioDeviceId);
	check(!IsStartedNonAtomic());
}

const FString& FBufferedSubmixListener::GetListenerName() const
{
	return Name;
}

void FBufferedSubmixListener::RegisterWithAudioDevice(FAudioDevice* InDevice)
{
	check(InDevice);

	DeviceId = InDevice->DeviceID;
	InDevice->RegisterSubmixBufferListener(AsShared(), InDevice->GetMainSubmixObject());
}

void FBufferedSubmixListener::UnregsiterWithAudioDevice(FAudioDevice* InDevice)
{
	check(InDevice);

	if (ensure(DeviceId != BufferedSubmixListenerPrivate::InvalidAudioDeviceId))
	{
		DeviceId = BufferedSubmixListenerPrivate::InvalidAudioDeviceId;
		InDevice->UnregisterSubmixBufferListener(AsShared(), InDevice->GetMainSubmixObject());
	}
}

bool FBufferedSubmixListener::Start(FAudioDevice* InAudioDevice)
{
	if (ensure(InAudioDevice))
	{
		if (TrySetStartedFlag())
		{
			RegisterWithAudioDevice(InAudioDevice);
		}
	}
	return false;
}

void FBufferedSubmixListener::Stop(FAudioDevice* InAudioDevice)
{
	if (ensure(InAudioDevice))
	{
		if (ensure(InAudioDevice->DeviceID == DeviceId))
		{
			if (TryUnsetStartedFlag())
			{
				UnregsiterWithAudioDevice(InAudioDevice);
			}
		}
	}
}

void FBufferedSubmixListener::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 InNumSamples, int32 InNumChannels, const int32 InSampleRate, double)
{
	if (IsStartedNonAtomic())
	{
		// Call to base class to handle.
		FBufferFormat NewFormat;
		NewFormat.NumChannels = InNumChannels;
		NewFormat.NumSamplesPerBlock = InNumSamples;
		NewFormat.NumSamplesPerSec = InSampleRate;
		OnBufferReceived(NewFormat, MakeArrayView(AudioData, InNumSamples));

		// Optionally, zero the buffer if we're asked to. This in the case where we're running both Unreal+Consumer renderers at once.
		// NOTE: this is dangerous as there's a chance we're not the only listener registered on this Submix. And will cause
		// listeners after us to have a silent buffer. Use with caution. 

		if (bZeroInputBuffer)
		{
			FMemory::Memzero(AudioData, sizeof(float) * InNumSamples);
		}	
	}
}
