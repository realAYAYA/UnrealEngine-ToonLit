// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceAudio.h"

#include "AudioDevice.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceAudio)

FNiagaraSubmixListener::FNiagaraSubmixListener(Audio::FPatchMixer& InMixer, int32 InNumSamplesToBuffer, Audio::FDeviceId InDeviceId, USoundSubmix* InSoundSubmix)
	: NumChannelsInSubmix(0)
	, SubmixSampleRate(0)
	, MixerInput(InMixer.AddNewInput(InNumSamplesToBuffer, 1.0f))
	, AudioDeviceId(InDeviceId)
	, Submix(InSoundSubmix)
	, bIsRegistered(false)
{
}

FNiagaraSubmixListener::FNiagaraSubmixListener()
	: NumChannelsInSubmix(0)
	, SubmixSampleRate(0)
	, MixerInput()
	, AudioDeviceId(INDEX_NONE)
	, Submix(nullptr)
	, bIsRegistered(false)
{
}

FNiagaraSubmixListener::FNiagaraSubmixListener(FNiagaraSubmixListener&& Other)
	: FNiagaraSubmixListener()
{
	UnregisterFromSubmix();
	Other.UnregisterFromSubmix();

	NumChannelsInSubmix = Other.NumChannelsInSubmix.Load();
	Other.NumChannelsInSubmix = 0;

	SubmixSampleRate = Other.SubmixSampleRate.Load();
	Other.SubmixSampleRate = 0;

	MixerInput = MoveTemp(Other.MixerInput);

	Submix = Other.Submix;
	Other.Submix = nullptr;

	AudioDeviceId = Other.AudioDeviceId;
	Other.AudioDeviceId = INDEX_NONE;

	RegisterToSubmix();
}

FNiagaraSubmixListener::~FNiagaraSubmixListener()
{
	UnregisterFromSubmix();
}

void FNiagaraSubmixListener::RegisterToSubmix()
{
	if (FAudioDevice* DeviceHandle = FAudioDeviceManager::Get()->GetAudioDeviceRaw(AudioDeviceId))
	{
		bIsRegistered = true;

		DeviceHandle->RegisterSubmixBufferListener(this, Submix);

		// RegisterSubmixBufferListener lazily enqueues the registration on the audio thread,
		// so we have to wait for the audio thread to destroy.
		FAudioCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();
	}
}

void FNiagaraSubmixListener::UnregisterFromSubmix()
{
	if (bIsRegistered)
	{
		bIsRegistered = false;
		
		if (FAudioDevice* DeviceHandle = FAudioDeviceManager::Get()->GetAudioDeviceRaw(AudioDeviceId))
		{
			if (IsInGameThread())
			{
				DeviceHandle->UnregisterSubmixBufferListener(this, Submix);
				
				// UnregisterSubmixBufferListener lazily enqueues the unregistration on the audio thread,
				// so we have to wait for the audio thread to destroy.
				FAudioCommandFence Fence;
				Fence.BeginFence();
				Fence.Wait();
			}
			else
			{
				UE::Tasks::FTaskEvent CompletionEvent{ UE_SOURCE_LOCATION };
				FAudioThread::RunCommandOnAudioThread([this, DeviceHandle, &CompletionEvent]()
				{
					DeviceHandle->UnregisterSubmixBufferListener(this, Submix);
					CompletionEvent.Trigger();
				});

				CompletionEvent.Wait();
			}
		}
	}
}

float FNiagaraSubmixListener::GetSampleRate() const
{
	return static_cast<float>(SubmixSampleRate.Load());
}

int32 FNiagaraSubmixListener::GetNumChannels() const
{
	return NumChannelsInSubmix;
}

void FNiagaraSubmixListener::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)
{
	NumChannelsInSubmix = NumChannels;
	SubmixSampleRate = SampleRate;
	MixerInput.PushAudio(AudioData, NumSamples);
}

///////////////////////////////////////////////////////////////////////////////////////////////////


FNiagaraDataInterfaceProxySubmix::FNiagaraDataInterfaceProxySubmix(int32 InNumSamplesToBuffer)
	: PatchMixer()
	, SubmixRegisteredTo(nullptr)
	, bIsSubmixListenerRegistered(false)
	, NumSamplesToBuffer(InNumSamplesToBuffer)
{
	check(NumSamplesToBuffer > 0);

	DeviceCreatedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddRaw(this, &FNiagaraDataInterfaceProxySubmix::OnNewDeviceCreated);
	DeviceDestroyedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddRaw(this, &FNiagaraDataInterfaceProxySubmix::OnDeviceDestroyed);
}

FNiagaraDataInterfaceProxySubmix::~FNiagaraDataInterfaceProxySubmix()
{
	FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(DeviceCreatedHandle);
	FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(DeviceDestroyedHandle);
}

void FNiagaraDataInterfaceProxySubmix::RegisterToAllAudioDevices()
{
	if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
	{
		// Register a new submix listener for every audio device that currently exists.
		DeviceManager->IterateOverAllDevices([&](Audio::FDeviceId DeviceId, FAudioDevice* InDevice)
		{
			AddSubmixListener(DeviceId);
		});
	}
}

void FNiagaraDataInterfaceProxySubmix::UnregisterFromAllAudioDevices()
{
	SubmixListeners.Empty();
}

void FNiagaraDataInterfaceProxySubmix::OnUpdateSubmix(USoundSubmix* Submix)
{
	if (bIsSubmixListenerRegistered)
	{
		UnregisterFromAllAudioDevices();
	}

	SubmixRegisteredTo = Submix;
	
	RegisterToAllAudioDevices();
	bIsSubmixListenerRegistered = true;
}


void FNiagaraDataInterfaceProxySubmix::OnNewDeviceCreated(Audio::FDeviceId InDeviceId)
{
	if (bIsSubmixListenerRegistered)
	{
		AddSubmixListener(InDeviceId);
	}
}

void FNiagaraDataInterfaceProxySubmix::OnDeviceDestroyed(Audio::FDeviceId InDeviceId)
{
	if (bIsSubmixListenerRegistered)
	{
		RemoveSubmixListener(InDeviceId);
	}
}

void FNiagaraDataInterfaceProxySubmix::AddSubmixListener(const Audio::FDeviceId InDeviceId)
{
	check(!SubmixListeners.Contains(InDeviceId));
	SubmixListeners.Emplace(InDeviceId, new FNiagaraSubmixListener(PatchMixer, NumSamplesToBuffer, InDeviceId, SubmixRegisteredTo));
	SubmixListeners[InDeviceId]->RegisterToSubmix();
}

void FNiagaraDataInterfaceProxySubmix::RemoveSubmixListener(Audio::FDeviceId InDeviceId)
{
	if (SubmixListeners.Contains(InDeviceId))
	{
		SubmixListeners.Remove(InDeviceId);
	}
}

int32 FNiagaraDataInterfaceProxySubmix::GetNumChannels() const
{
	TArray<Audio::FDeviceId> DeviceIds;
	SubmixListeners.GetKeys(DeviceIds);
	for (const Audio::FDeviceId DeviceId : DeviceIds)
	{
		int32 NumChannels = SubmixListeners[DeviceId]->GetNumChannels();

		if (NumChannels != 0)
		{
			// Not sure which listener is receiving callbacks, or if more than one is,
			// so return first non-zero value.
			return NumChannels;
		}
	}
	return 0;
}

float FNiagaraDataInterfaceProxySubmix::GetSampleRate() const
{
	TArray<Audio::FDeviceId> DeviceIds;
	SubmixListeners.GetKeys(DeviceIds);
	for (const Audio::FDeviceId DeviceId : DeviceIds)
	{
		float SampleRate = SubmixListeners[DeviceId]->GetSampleRate();

		if (SampleRate > 0.f)
		{
			// Not sure which listener is receiving callbacks, or if more than one is,
			// so return first non-zero value.
			return SampleRate;
		}
	}
	return 0.f;
}

void FNiagaraDataInterfaceProxySubmix::OnBeginDestroy()
{
	if (bIsSubmixListenerRegistered)
	{
		UnregisterFromAllAudioDevices();
	}
}

int32 FNiagaraDataInterfaceProxySubmix::PopAudio(float* OutBuffer, int32 NumSamples, bool bUseLatestAudio)
{
	return PatchMixer.PopAudio(OutBuffer, NumSamples, bUseLatestAudio);
}

int32 FNiagaraDataInterfaceProxySubmix::GetNumSamplesAvailable()
{
	return PatchMixer.MaxNumberOfSamplesThatCanBePopped();
}

int32 FNiagaraDataInterfaceProxySubmix::GetNumFramesAvailable()
{
	int32 NumChannels = GetNumChannels();

	if (NumChannels < 1)
	{
		return 0;
	}

	return GetNumSamplesAvailable() / NumChannels;
}

int32 FNiagaraDataInterfaceProxySubmix::PerInstanceDataPassedToRenderThreadSize() const
{
	return 0;
}

UNiagaraDataInterfaceAudioSubmix::UNiagaraDataInterfaceAudioSubmix(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Submix(nullptr)
{
	int32 DefaultNumSamplesToBuffer = 16384;
	Proxy = MakeUnique<FNiagaraDataInterfaceProxySubmix>(DefaultNumSamplesToBuffer);
}

bool UNiagaraDataInterfaceAudioSubmix::Equals(const UNiagaraDataInterface* Other) const
{
	bool bIsEqual = Super::Equals(Other);

	const UNiagaraDataInterfaceAudioSubmix* OtherSubmix = CastChecked<const UNiagaraDataInterfaceAudioSubmix>(Other);

	bIsEqual &= OtherSubmix->Submix == Submix;

	return bIsEqual;
}

#if WITH_EDITOR
void UNiagaraDataInterfaceAudioSubmix::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	static FName SubmixFName = GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceAudioSubmix, Submix);

	// Regenerate on save any compressed sound formats or if analysis needs to be re-done
	if (FProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		const FName& Name = PropertyThatChanged->GetFName();
		if (Name == SubmixFName)
		{
			GetProxyAs<FNiagaraDataInterfaceProxySubmix>()->OnUpdateSubmix(Submix);
		}
	}
}
#endif //WITH_EDITOR

void UNiagaraDataInterfaceAudioSubmix::PostInitProperties()
{
	Super::PostInitProperties();
}

void UNiagaraDataInterfaceAudioSubmix::BeginDestroy()
{
	GetProxyAs<FNiagaraDataInterfaceProxySubmix>()->OnBeginDestroy();

	Super::BeginDestroy();
}

void UNiagaraDataInterfaceAudioSubmix::PostLoad()
{
	Super::PostLoad();

	GetProxyAs<FNiagaraDataInterfaceProxySubmix>()->OnUpdateSubmix(Submix);
}

bool UNiagaraDataInterfaceAudioSubmix::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	Super::CopyToInternal(Destination);

	UNiagaraDataInterfaceAudioSubmix* CastedDestination = Cast<UNiagaraDataInterfaceAudioSubmix>(Destination);

	if (CastedDestination)
	{
		CastedDestination->Submix = Submix;
		CastedDestination->GetProxyAs<FNiagaraDataInterfaceProxySubmix>()->OnUpdateSubmix(Submix);
	}
	
	return true;
}


