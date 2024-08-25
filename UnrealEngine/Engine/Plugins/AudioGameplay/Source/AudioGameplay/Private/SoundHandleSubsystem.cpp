// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundHandleSubsystem.h"

#include "ActiveSound.h"
#include "AudioDevice.h"
#include "Audio/ISoundHandleOwner.h"

bool USoundHandleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer();
}

void USoundHandleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

void USoundHandleSubsystem::Deinitialize()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	ActiveHandles.Reset();
	Super::Deinitialize();
}

void USoundHandleSubsystem::OnNotifyPendingDelete(const FActiveSound& ActiveSound)
{
	check(IsInAudioThread());
	if (ActiveHandles.Num() > 0)
	{
		const Audio::FSoundHandleID HandleID = ActiveSound.GetInstanceID();
		ActiveHandles.Remove(HandleID);
		if(ISoundHandleOwner* Owner = Owners.FindAndRemoveChecked(HandleID))
		{			
			Owner->OnSoundHandleRemoved();
		}
	}
}

Audio::FSoundHandleID USoundHandleSubsystem::CreateSoundHandle(USoundBase* Sound, ISoundHandleOwner* Owner)
{
	if(Sound && Owner)
	{
		FSoundHandle NewHandle;
		NewHandle.ActiveSound.bLocationDefined = false;
		Audio::FSoundHandleID HandleID = NewHandle.ActiveSound.GetInstanceID();
		NewHandle.ActiveSound.SetSound(Sound);
		ActiveHandles.Emplace(HandleID, NewHandle);
		Owners.Emplace(HandleID, Owner);
		return HandleID;
	}
	return INDEX_NONE;
}

void USoundHandleSubsystem::SetTransform(const Audio::FSoundHandleID ID, const FTransform& Transform)
{
	// TODO: We should add this as an incoming change to handle on the audio thread in the update
	if(FSoundHandle* Handle = ActiveHandles.Find(ID))
	{
		Handle->ActiveSound.bLocationDefined = true;
		Handle->ActiveSound.Transform = Transform;
	}	
}

Audio::EResult USoundHandleSubsystem::Play(const Audio::FSoundHandleID ID)
{
	if(FSoundHandle* Handle = ActiveHandles.Find(ID))
	{
		FAudioDevice* AudioDevice = GetAudioDeviceHandle().GetAudioDevice();
		if (Handle->ActiveSound.bLocationDefined)
		{
			float MaxDistance = 0.0f;
			float FocusFactor = 1.0f;
			const FVector Location = Handle->ActiveSound.Transform.GetLocation();

			const TObjectPtr<USoundBase> Sound = Handle->ActiveSound.GetSound();
			const FSoundAttenuationSettings* AttenuationSettings = Sound->GetAttenuationSettingsToApply();
			AudioDevice->GetMaxDistanceAndFocusFactor(Sound, GetWorld(), Location, AttenuationSettings, MaxDistance, FocusFactor);
			
			Handle->ActiveSound.bHasAttenuationSettings = (AttenuationSettings != nullptr);
			if (Handle->ActiveSound.bHasAttenuationSettings)
			{
				Handle->ActiveSound.AttenuationSettings = *AttenuationSettings;
				Handle->ActiveSound.FocusData.PriorityScale = AttenuationSettings->GetFocusPriorityScale(GetAudioDeviceHandle().GetAudioDevice()->GetGlobalFocusSettings(), FocusFactor);
			}

			Handle->ActiveSound.MaxDistance = MaxDistance;
		}
		
		AudioDevice->AddNewActiveSound(Handle->ActiveSound);		
		return Audio::EResult::Success;
	}
	
	return Audio::EResult::Failure;
}

void USoundHandleSubsystem::Stop(const Audio::FSoundHandleID ID)
{	
	if(FSoundHandle* Handle = ActiveHandles.Find(ID))
	{
		FAudioDevice* AudioDevice = GetAudioDeviceHandle().GetAudioDevice();
		AudioDevice->StopActiveSound(&Handle->ActiveSound);
	}
}
