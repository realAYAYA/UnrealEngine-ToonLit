// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioGameplayVolumeSubsystem.h"
#include "AudioGameplayVolumeLogs.h"
#include "AudioGameplayVolumeComponent.h"
#include "AudioGameplayVolumeProxy.h"
#include "AudioGameplayVolumeMutator.h"
#include "AudioDevice.h"
#include "Misc/CoreMisc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioGameplayVolumeSubsystem)

namespace AudioGameplayVolumeConsoleVariables
{
	int32 bEnabled = 1;
	FAutoConsoleVariableRef CVarEnabled(
		TEXT("au.AudioGameplayVolumes.Enabled"),
		bEnabled,
		TEXT("Toggles the Audio Gameplay Volume System on or off.\n0: Disable, 1: Enable (default)"),
		ECVF_Default);

	int32 bUpdateListeners = 1;
	FAutoConsoleVariableRef CVarUpdateListeners(
		TEXT("au.AudioGameplayVolumes.Listeners.AllowUpdate"),
		bUpdateListeners,
		TEXT("Allows updating of listeners.\n0: Disable, 1: Enable (default)"),
		ECVF_Default);

	float MinUpdateRate = 0.016f;
	float UpdateRate = 0.05f;
	FAutoConsoleVariableRef CVarUpdateInterval(
		TEXT("au.AudioGameplayVolume.UpdateRate"),
		UpdateRate,
		TEXT("How frequently we check for listener changes with respect to audio gameplay volumes, in seconds."),
		ECVF_Default);

	float UpdateRateJitterDelta = 0.025f;
	FAutoConsoleVariableRef CVarUpdateRateJitterDelta(
		TEXT("au.AudioGameplayVolume.UpdateRate.JitterDelta"),
		UpdateRateJitterDelta,
		TEXT("A random delta to add to update rate to avoid performance heartbeats."),
		ECVF_Default);

} // namespace AudioGameplayVolumeConsoleVariables

void FAudioGameplayActiveSoundInfo::Update(double ListenerInteriorStartTime)
{
	// If the interior settings have changed for the listener or the sound, we need to set
	// new interpolation targets.  However, we need to 'initialize' the sounds interior start time to match
	// the listener's interior start time, to prevent new sounds from interpolating until they change interior settings.
	if (FMath::IsNearlyZero(LastUpdateTime))
	{
		InteriorSettings.SetInteriorStartTime(ListenerInteriorStartTime);
	}

	if (LastUpdateTime < ListenerInteriorStartTime || LastUpdateTime < InteriorSettings.GetInteriorStartTime())
	{
		SourceInteriorVolume = CurrentInteriorVolume;
		SourceInteriorLPF = CurrentInteriorLPF;
		LastUpdateTime = FApp::GetCurrentTime();
	}

	InteriorSettings.UpdateInteriorValues();
}

void FAudioProxyMutatorSearchObject::SearchVolumes(const TArray<TWeakObjectPtr<UAudioGameplayVolumeProxy>>& ProxyVolumes, FAudioProxyMutatorSearchResult& OutResult)
{
	check(IsInAudioThread());
	SCOPED_NAMED_EVENT(FAudioProxyMutatorSearchObject_SearchVolumes, FColor::Blue);

	OutResult.Reset();
	FAudioProxyMutatorPriorities MutatorPriorities;
	MutatorPriorities.PayloadType = PayloadType;
	MutatorPriorities.bFilterPayload = bFilterPayload;

	for (const TWeakObjectPtr<UAudioGameplayVolumeProxy>& ProxyVolume : ProxyVolumes)
	{
		if (!ProxyVolume.IsValid() || ProxyVolume->GetWorldID() != WorldID)
		{
			continue;
		}

		if (bFilterPayload && !ProxyVolume->HasPayloadType(PayloadType))
		{
			continue;
		}

		if (!ProxyVolume->ContainsPosition(Location))
		{
			continue;
		}

		if (bCollectMutators)
		{
			// We only need to calculate priorities for mutators if we're collecting them.
			ProxyVolume->FindMutatorPriority(MutatorPriorities);
		}
		OutResult.VolumeSet.Add(ProxyVolume->GetVolumeID());
	}

	// Use 'world settings' as a starting point
	if (!bAffectedByLegacySystem && AudioDeviceHandle.IsValid())
	{
		AudioDeviceHandle->GetDefaultAudioSettings(WorldID, OutResult.ReverbSettings, OutResult.InteriorSettings);
	}

	if (bCollectMutators)
	{
		for (const TWeakObjectPtr<UAudioGameplayVolumeProxy>& ProxyVolume : ProxyVolumes)
		{
			if (!ProxyVolume.IsValid())
			{
				continue;
			}

			if (OutResult.VolumeSet.Contains(ProxyVolume->GetVolumeID()))
			{
				ProxyVolume->GatherMutators(MutatorPriorities, OutResult);
			}
		}
	}
}

void FAudioGameplayVolumeProxyInfo::Update(const TArray<FAudioGameplayVolumeListener>& VolumeListeners, FAudioGameplayProxyUpdateResult& OutResult)
{
	check(IsInAudioThread());

	PreviousProxies = MoveTemp(CurrentProxies);
	for (const int32 ListenerIndex : ListenerIndexes)
	{
		if (ListenerIndex <= VolumeListeners.Num())
		{
			CurrentProxies.Append(VolumeListeners[ListenerIndex].GetCurrentProxies());
		}
	}

	ListenerIndexes.Reset();

	// We've entered proxies that are in the current list, but not in previous
	OutResult.EnteredProxies = CurrentProxies.Difference(PreviousProxies);

	// We've exited proxies that are in the previous list, but not in current
	OutResult.ExitedProxies = PreviousProxies.Difference(CurrentProxies);
}

void FAudioGameplayVolumeProxyInfo::AddListenerIndex(int32 ListenerIndex)
{
	ListenerIndexes.Emplace(ListenerIndex);
}

bool FAudioGameplayVolumeProxyInfo::IsVolumeInCurrentList(uint32 VolumeID) const
{
	return CurrentProxies.Contains(VolumeID);
}

bool UAudioGameplayVolumeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer();
}

void UAudioGameplayVolumeSubsystem::Update()
{
	check(IsInAudioThread());
	
	// We track and check our previous proxy count to ensure we get one update after all of the volumes have been removed
	if (AudioGameplayVolumeConsoleVariables::bEnabled == 0 || (ProxyVolumes.Num() == 0 && PreviousProxyCount == 0))
	{
		return;
	}

	// limit updates for perf - not necessary to update every tick
	const float DeltaTime = FApp::GetDeltaTime();
	TimeSinceUpdate += DeltaTime;
	if (TimeSinceUpdate < NextUpdateDeltaTime)
	{
		return;
	}

	const float JitterDelta = FMath::RandRange(0.f, AudioGameplayVolumeConsoleVariables::UpdateRateJitterDelta);
	NextUpdateDeltaTime = FMath::Max(AudioGameplayVolumeConsoleVariables::UpdateRate + JitterDelta, AudioGameplayVolumeConsoleVariables::MinUpdateRate);
	TimeSinceUpdate = 0.f;

	if (AudioGameplayVolumeConsoleVariables::bUpdateListeners != 0)
	{
		UpdateFromListeners();
	}

	PreviousProxyCount = ProxyVolumes.Num();
}

void UAudioGameplayVolumeSubsystem::GatherInteriorData(const FActiveSound& ActiveSound, FSoundParseParameters& ParseParams)
{
	if (AudioGameplayVolumeConsoleVariables::bEnabled == 0)
	{
		return;
	}

	check(IsInAudioThread());

	FAudioProxyMutatorSearchObject MutatorSearch;
	MutatorSearch.WorldID = ActiveSound.GetWorldID();
	MutatorSearch.Location = ParseParams.Transform.GetTranslation();
	MutatorSearch.PayloadType = AudioGameplay::EComponentPayload::AGCP_ActiveSound;
	MutatorSearch.AudioDeviceHandle = GetAudioDeviceHandle();
	MutatorSearch.bAffectedByLegacySystem = (ActiveSound.AudioVolumeID != 0);

	FAudioProxyMutatorSearchResult Result;
	MutatorSearch.SearchVolumes(ProxyVolumes, Result);

	// Save info about this active sound for application.
	FAudioGameplayActiveSoundInfo& ActiveSoundInfo = ActiveSoundData.FindOrAdd(ActiveSound.GetInstanceID());
	ActiveSoundInfo.CurrentMutators = MoveTemp(Result.MatchingMutators);
	ActiveSoundInfo.InteriorSettings.Apply(Result.InteriorSettings);
}

void UAudioGameplayVolumeSubsystem::ApplyInteriorSettings(const FActiveSound& ActiveSound, FSoundParseParameters& ParseParams)
{
	if (AudioGameplayVolumeConsoleVariables::bEnabled == 0)
	{
		return;
	}

	check(IsInAudioThread());

	int32 ListenerIndex = ActiveSound.GetClosestListenerIndex();
	if (ListenerIndex >= AGVListeners.Num())
	{
		return;
	}

	// Legacy audio volumes is affecting this sound, do not update
	const FAudioGameplayVolumeListener& Listener = AGVListeners[ListenerIndex];
	if(ActiveSound.AudioVolumeID != 0 || Listener.GetAffectedByLegacySystem())
	{
		return;
	}

	FAudioGameplayActiveSoundInfo* ActiveSoundInfo = ActiveSoundData.Find(ActiveSound.GetInstanceID());
	if (!ActiveSoundInfo)
	{
		return;
	}

	ActiveSoundInfo->Update(Listener.GetInteriorSettings().GetInteriorStartTime());

	uint32 WorldID = ActiveSound.GetWorldID();
	FAudioProxyActiveSoundParams Params(*ActiveSoundInfo, Listener);
	Params.bAllowSpatialization = ActiveSound.bAllowSpatialization;

	for (const TSharedPtr<FProxyVolumeMutator>& SoundMutator : ActiveSoundInfo->CurrentMutators)
	{
		if (SoundMutator.IsValid())
		{
			Params.bListenerInVolume = IsAnyListenerInVolume(WorldID, SoundMutator->VolumeID);
			SoundMutator->Apply(Params);
		}
	}

	// Update interior values
	Params.UpdateInteriorValues();
	ActiveSoundInfo->CurrentInteriorVolume = Params.SourceInteriorVolume;
	ActiveSoundInfo->CurrentInteriorLPF = Params.SourceInteriorLPF;

	// Apply to our parse params
	ParseParams.SoundSubmixSends.Append(Params.SoundSubmixSends);
	ParseParams.InteriorVolumeMultiplier = ActiveSoundInfo->CurrentInteriorVolume;
	ParseParams.AmbientZoneFilterFrequency = ActiveSoundInfo->CurrentInteriorLPF;
}

void UAudioGameplayVolumeSubsystem::OnNotifyPendingDelete(const FActiveSound& ActiveSound)
{
	check(IsInAudioThread());
	if (ActiveSound.bApplyInteriorVolumes && ActiveSoundData.Num() > 0)
	{
		ActiveSoundData.Remove(ActiveSound.GetInstanceID());
	}
}

void UAudioGameplayVolumeSubsystem::AddVolumeComponent(const UAudioGameplayVolumeComponent* VolumeComponent)
{
	if (!VolumeComponent)
	{
		UE_LOG(AudioGameplayVolumeLog, Verbose, TEXT("AudioGameplayVolumeSubsystem - Attempting to add invalid volume component!"));
		return;
	}
	
	if (!DoesSupportWorld(VolumeComponent->GetWorld()))
	{
		return;
	}

	const uint32 ComponentID = VolumeComponent->GetUniqueID();
	UAudioGameplayVolumeProxy* VolumeProxy = VolumeComponent->GetProxy();
	if (VolumeProxy && !AGVComponents.Contains(ComponentID))
	{
		VolumeProxy->InitFromComponent(VolumeComponent);
		AGVComponents.Emplace(ComponentID, VolumeComponent);

		UE_LOG(AudioGameplayVolumeLog, VeryVerbose, TEXT("AudioGameplayVolumeComponent %s [%08x] added"), *VolumeComponent->GetFName().ToString(), ComponentID);

		// Copy representation of volume to audio thread
		TWeakObjectPtr<UAudioGameplayVolumeProxy> WeakProxy(VolumeProxy);
		TWeakObjectPtr<UAudioGameplayVolumeSubsystem> WeakThis(this);
		FAudioThread::RunCommandOnAudioThread([WeakThis, WeakProxy]()
		{
			if (WeakThis.IsValid() && WeakProxy.IsValid())
			{
				WeakThis->AddProxy(WeakProxy);
			}
		});
	}
}

void UAudioGameplayVolumeSubsystem::UpdateVolumeComponent(const UAudioGameplayVolumeComponent* VolumeComponent)
{
	if (!VolumeComponent || !VolumeComponent->GetWorld())
	{
		UE_LOG(AudioGameplayVolumeLog, Verbose, TEXT("AudioGameplayVolumeSubsystem - Attempting to update invalid volume component!"));
		return;
	}

	const uint32 ComponentID = VolumeComponent->GetUniqueID();
	UAudioGameplayVolumeProxy* VolumeProxy = VolumeComponent->GetProxy();
	if (VolumeProxy && AGVComponents.Contains(ComponentID))
	{
		VolumeProxy->InitFromComponent(VolumeComponent);

		UE_LOG(AudioGameplayVolumeLog, VeryVerbose, TEXT("AudioGameplayVolumeComponent %s [%08x] updated"), *VolumeComponent->GetFName().ToString(), ComponentID);

		TWeakObjectPtr<UAudioGameplayVolumeProxy> WeakProxy(VolumeProxy);
		TWeakObjectPtr<UAudioGameplayVolumeSubsystem> WeakThis(this);
		FAudioThread::RunCommandOnAudioThread([WeakThis, WeakProxy]()
		{
			if (WeakThis.IsValid() && WeakProxy.IsValid())
			{
				WeakThis->UpdateProxy(WeakProxy);
			}
		});
	}
}

void UAudioGameplayVolumeSubsystem::RemoveVolumeComponent(const UAudioGameplayVolumeComponent* VolumeComponent)
{
	if (!VolumeComponent || !VolumeComponent->GetWorld())
	{
		UE_LOG(AudioGameplayVolumeLog, Verbose, TEXT("AudioGameplayVolumeSubsystem - Attempting to remove invalid volume component!"));
		return;
	}

	const uint32 ComponentID = VolumeComponent->GetUniqueID();
	if (AGVComponents.Contains(ComponentID))
	{
		AGVComponents.Remove(ComponentID);
		UE_LOG(AudioGameplayVolumeLog, VeryVerbose, TEXT("AudioGameplayVolumeComponent %s [%08x] removed"), *VolumeComponent->GetFName().ToString(), ComponentID);
	}

	// Remove representation of volume from audio thread
	TWeakObjectPtr<UAudioGameplayVolumeSubsystem> WeakThis(this);
	FAudioThread::RunCommandOnAudioThread([WeakThis, ComponentID]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->RemoveProxy(ComponentID);
		}
	});
}

bool UAudioGameplayVolumeSubsystem::DoesSupportWorld(UWorld* World) const
{
	if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
	{
		return true;
	}
	return false;
}

bool UAudioGameplayVolumeSubsystem::AddProxy(TWeakObjectPtr<UAudioGameplayVolumeProxy> WeakProxy)
{
	check(IsInAudioThread());
	check(WeakProxy.IsValid());

	// Only add to the Proxy array if the element doesn't already exist
	const uint32 NewVolumeID = WeakProxy->GetVolumeID();
	if (!ProxyVolumes.FindByPredicate([NewVolumeID](const TWeakObjectPtr<UAudioGameplayVolumeProxy>& Proxy) { return Proxy.IsValid() && Proxy->GetVolumeID() == NewVolumeID; }))
	{
		ProxyVolumes.Emplace(WeakProxy);

		FAudioGameplayVolumeProxyInfo& ProxyInfo = WorldProxyLists.FindOrAdd(WeakProxy->GetWorldID());
		if (ProxyInfo.IsVolumeInCurrentList(NewVolumeID))
		{
			bHasStaleProxy = true;
		}

		UE_LOG(AudioGameplayVolumeLog, VeryVerbose, TEXT("Proxy [%08x] added"), NewVolumeID);
		return true;
	}

	UE_LOG(AudioGameplayVolumeLog, VeryVerbose, TEXT("Attempting to add Proxy [%08x] multiple times"), NewVolumeID);
	return false;
}

bool UAudioGameplayVolumeSubsystem::UpdateProxy(TWeakObjectPtr<UAudioGameplayVolumeProxy> WeakProxy)
{
	check(IsInAudioThread());
	check(WeakProxy.IsValid());

	const uint32 ProxyVolumeID = WeakProxy->GetVolumeID();
	TWeakObjectPtr<UAudioGameplayVolumeProxy>* ProxyPtr = ProxyVolumes.FindByPredicate([ProxyVolumeID](const TWeakObjectPtr<UAudioGameplayVolumeProxy>& Proxy)
	{
		return Proxy.IsValid() && Proxy->GetVolumeID() == ProxyVolumeID;
	});

	if (ProxyPtr)
	{
		*ProxyPtr = WeakProxy;
		bHasStaleProxy = true;
		return true;
	}

	return false;
}

bool UAudioGameplayVolumeSubsystem::RemoveProxy(uint32 AudioGameplayVolumeID)
{
	check(IsInAudioThread());
	const int32 NumRemoved = ProxyVolumes.RemoveAllSwap([AudioGameplayVolumeID](const TWeakObjectPtr<UAudioGameplayVolumeProxy>& Proxy)
	{
		return !Proxy.IsValid() || Proxy->GetVolumeID() == AudioGameplayVolumeID;
	});

	UE_LOG(AudioGameplayVolumeLog, VeryVerbose, TEXT("Removed %d Proxies with Id [%08x]"), NumRemoved, AudioGameplayVolumeID);
	return NumRemoved > 0;
}

bool UAudioGameplayVolumeSubsystem::IsAnyListenerInVolume(uint32 WorldID, uint32 VolumeID) const
{
	// We test this by checking to see if the volume id provided is in our world's current proxy list
	if (const FAudioGameplayVolumeProxyInfo* ProxyInfo = WorldProxyLists.Find(WorldID))
	{
		return ProxyInfo->IsVolumeInCurrentList(VolumeID);
	}
	return false;
}

void UAudioGameplayVolumeSubsystem::UpdateComponentsFromProxyInfo(const FAudioGameplayProxyUpdateResult& ProxyResults) const
{
	check(IsInGameThread());
	for (uint32 VolumeID : ProxyResults.EnteredProxies)
	{
		if (!AGVComponents.Contains(VolumeID))
		{
			continue;
		}

		if (const UAudioGameplayVolumeComponent* ProxyComponent = AGVComponents[VolumeID])
		{
			ProxyComponent->EnterProxy();
		}
	}

	for (uint32 VolumeID : ProxyResults.ExitedProxies)
	{
		if (!AGVComponents.Contains(VolumeID))
		{
			continue;
		}

		if (const UAudioGameplayVolumeComponent* ProxyComponent = AGVComponents[VolumeID])
		{
			ProxyComponent->ExitProxy();
		}
	}
}

void UAudioGameplayVolumeSubsystem::UpdateFromListeners()
{
	check(IsInAudioThread());
	SCOPED_NAMED_EVENT(UAudioGameplayVolumeSubsystem_UpdateFromListeners, FColor::Blue);

	FAudioDeviceHandle DeviceHandle = GetAudioDeviceHandle();
	check(DeviceHandle.IsValid());
	const uint32 AudioDeviceID = DeviceHandle.GetDeviceID();

	constexpr bool bAllowShrink = false;
	const int32 ListenerCount = DeviceHandle->GetListeners().Num();
	AGVListeners.SetNum(ListenerCount, bAllowShrink);
	constexpr bool bAllowAttenuationOverride = true;

	// We have to search twice, as we only care about mutators that affect listeners, but we care about ALL proxyVolumes we're a part of
	FAudioProxyMutatorSearchObject ProxySearch;
	ProxySearch.AudioDeviceHandle = DeviceHandle;
	ProxySearch.PayloadType = AudioGameplay::EComponentPayload::AGCP_Listener;

	FAudioProxyMutatorSearchResult Result;
	TSet<uint32> TempVolumeSet;
	FTransform ListenerTransform;

	// Grabbing the listeners directly here should be removed when possible - done out of necessity due to the legacy audio volume system
	const TArray<FListener>& AudioListeners = DeviceHandle->GetListeners();

	// Update our audio gameplay volume listeners
	for (int32 i = 0; i < ListenerCount; ++i)
	{
		const bool bValidAudioListener = DeviceHandle->GetListenerTransform(i, ListenerTransform);
		if (bValidAudioListener && !ListenerTransform.Equals(FTransform::Identity))
		{
			// Fill location and worldID
			if (DeviceHandle->GetListenerWorldID(i, ProxySearch.WorldID) && DeviceHandle->GetListenerPosition(i, ProxySearch.Location, bAllowAttenuationOverride))
			{
				// Find only the proxy volumes we're inside of, regardless of payload type
				ProxySearch.bFilterPayload = false;
				ProxySearch.bCollectMutators = false;
				ProxySearch.bGetDefaultAudioSettings = false;
				ProxySearch.SearchVolumes(ProxyVolumes, Result);

				// Hold on to these
				Swap(TempVolumeSet, Result.VolumeSet);

				// Second search - this time for mutators
				ProxySearch.bFilterPayload = true;
				ProxySearch.bCollectMutators = true;
				ProxySearch.bGetDefaultAudioSettings = true;
				ProxySearch.SearchVolumes(ProxyVolumes, Result);

				if (i < AudioListeners.Num())
				{
					AGVListeners[i].SetAffectedByLegacySystem(AudioListeners[i].AudioVolumeID != 0);
				}

				// Reassign the set of all proxy volumes (regardless of payload type)
				Swap(TempVolumeSet, Result.VolumeSet);
				AGVListeners[i].Update(Result, ProxySearch.Location, AudioDeviceID);

				if (FAudioGameplayVolumeProxyInfo* ProxyInfo = WorldProxyLists.Find(ProxySearch.WorldID))
				{
					ProxyInfo->AddListenerIndex(i);
				}

				continue;
			}
		}

		// Listener is invalid or uninitialized
		Result.Reset();
		AGVListeners[i].Update(Result, FVector::ZeroVector, AudioDeviceID);
	}

	FAudioGameplayProxyUpdateResult ProxyUpdateResult;
	TWeakObjectPtr<UAudioGameplayVolumeSubsystem> WeakThis(this);

	// Update our world proxy lists
	for (TPair<uint32, FAudioGameplayVolumeProxyInfo>& WorldProxyInfo : WorldProxyLists)
	{
		WorldProxyInfo.Value.Update(AGVListeners, ProxyUpdateResult);
		FAudioThread::RunCommandOnGameThread([WeakThis, ProxyUpdateResult]
		{
			if (WeakThis.IsValid())
			{
				WeakThis->UpdateComponentsFromProxyInfo(ProxyUpdateResult);
			}
		});
	}
}

