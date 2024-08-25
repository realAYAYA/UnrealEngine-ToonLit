// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Transition/AvaPlaybackServerTransition.h"

#include "IAvaMediaModule.h"
#include "Playable/Transition/AvaPlayableTransition.h"
#include "Playback/AvaPlaybackManager.h"
#include "Playback/AvaPlaybackServer.h"

namespace UE::AvaPlaybackServerTransition::Private
{
	FString GetPrettyPlaybackInstanceInfo(const FAvaPlaybackInstance* InPlaybackInstance)
	{
		if (InPlaybackInstance)
		{
			return FString::Printf(TEXT("Id:%s, Asset:%s, Channel:%s, UserData:\"%s\""),
				*InPlaybackInstance->GetInstanceId().ToString(),
				*InPlaybackInstance->GetSourcePath().ToString(),
				*InPlaybackInstance->GetChannelName(),
				*InPlaybackInstance->GetInstanceUserData());
		}
		return TEXT("");
	}

	UAvaPlayable* GetPlayable(const FAvaPlaybackInstance* InPlaybackInstance)
	{
		if (InPlaybackInstance && InPlaybackInstance->GetPlayback())
		{
			return InPlaybackInstance->GetPlayback()->GetFirstPlayable();
		}
		return nullptr;
	}

	TSharedPtr<FAvaPlaybackInstance> FindInstanceForPlayable(const TArray<TWeakPtr<FAvaPlaybackInstance>>& InPlaybackInstancesWeak, const UAvaPlayable* InPlayable)
	{
		for (const TWeakPtr<FAvaPlaybackInstance>& InstanceWeak : InPlaybackInstancesWeak)
		{
			TSharedPtr<FAvaPlaybackInstance> Instance = InstanceWeak.Pin();
			if (GetPlayable(Instance.Get()) == InPlayable)
			{
				return Instance;
			}
		}
		return nullptr;
	}
}

void UAvaPlaybackServerTransition::SetEnterValues(const TArray<FAvaPlayableRemoteControlValues>& InEnterValues)
{
	EnterValues.Reserve(InEnterValues.Num());
	for (const FAvaPlayableRemoteControlValues& Values : InEnterValues)
	{
		EnterValues.Add(MakeShared<FAvaPlayableRemoteControlValues>(Values));
	}
}

bool UAvaPlaybackServerTransition::AddEnterInstance(const TSharedPtr<FAvaPlaybackInstance>& InPlaybackInstance)
{
	if (!InPlaybackInstance)
	{
		return false;
	}
	
	// Register this transition as a visibility constraint.
	using namespace UE::AvaPlaybackServerTransition::Private;
	if (const UAvaPlayable* Playable = GetPlayable(InPlaybackInstance.Get()))
	{
		if (UAvaPlayableGroup* PlayableGroup = Playable->GetPlayableGroup())
		{
			PlayableGroup->RegisterVisibilityConstraint(this);
		}
	}
	else if (UAvaPlaybackGraph* Playback = InPlaybackInstance->GetPlayback())
	{
		// If the playable is not created yet, register to the creation event.
		Playback->OnPlayableCreated.AddUObject(this, &UAvaPlaybackServerTransition::OnPlayableCreated);
	}
	
	return AddPlaybackInstance(InPlaybackInstance, EnterPlaybackInstancesWeak);
}

bool UAvaPlaybackServerTransition::AddPlayingInstance(const TSharedPtr<FAvaPlaybackInstance>& InPlaybackInstance)
{
	return AddPlaybackInstance(InPlaybackInstance, PlayingPlaybackInstancesWeak);
}

bool UAvaPlaybackServerTransition::AddExitInstance(const TSharedPtr<FAvaPlaybackInstance>& InPlaybackInstance)
{
	return AddPlaybackInstance(InPlaybackInstance, ExitPlaybackInstancesWeak);
}

void UAvaPlaybackServerTransition::TryResolveInstances(const FAvaPlaybackServer& InPlaybackServer)
{
	if (EnterPlaybackInstancesWeak.Num() < EnterInstanceIds.Num())
	{
		for (const FGuid& InstanceId : EnterInstanceIds)
		{
			if (TSharedPtr<FAvaPlaybackInstance> Instance = InPlaybackServer.FindActivePlaybackInstance(InstanceId))
			{
				if (!EnterPlaybackInstancesWeak.Contains(Instance.ToWeakPtr()))
				{
					AddEnterInstance(Instance);
				}
			}
		}
	}	
}

bool UAvaPlaybackServerTransition::IsVisibilityConstrained(const UAvaPlayable* InPlayable) const
{
	using namespace UE::AvaPlaybackServerTransition::Private;
	bool bAllPlayablesLoaded = true;
	bool bIsPlayableInThisTransition = false;
	
	for (const TWeakPtr<FAvaPlaybackInstance>& InstanceWeak : EnterPlaybackInstancesWeak)
	{
		if (const TSharedPtr<FAvaPlaybackInstance> Instance = InstanceWeak.Pin())
		{
			if (const UAvaPlayable* Playable = GetPlayable(Instance.Get()))
			{
				if (Playable == InPlayable)
				{
					bIsPlayableInThisTransition = true;
				}
				const EAvaPlayableStatus PlayableStatus = Playable->GetPlayableStatus();
				if (PlayableStatus != EAvaPlayableStatus::Loaded && PlayableStatus != EAvaPlayableStatus::Visible)
				{
					bAllPlayablesLoaded = false;
				}
			}
		}
	}
	return bIsPlayableInThisTransition && !bAllPlayablesLoaded;
}


bool UAvaPlaybackServerTransition::CanStart(bool& bOutShouldDiscard) const
{
	using namespace UE::AvaPlaybackServerTransition::Private;

	if (EnterPlaybackInstancesWeak.Num() < EnterInstanceIds.Num())
	{
		bOutShouldDiscard = false;
		return false;
	}

	for (const TWeakPtr<FAvaPlaybackInstance>& InstanceWeak : EnterPlaybackInstancesWeak)
	{
		if (const TSharedPtr<FAvaPlaybackInstance> Instance = InstanceWeak.Pin())
		{
			if (const UAvaPlayable* Playable = GetPlayable(Instance.Get()))
			{
				const EAvaPlayableStatus PlayableStatus = Playable->GetPlayableStatus();

				if (PlayableStatus == EAvaPlayableStatus::Unknown
					|| PlayableStatus == EAvaPlayableStatus::Error)
				{
					// Discard the command
					bOutShouldDiscard = true;
					return false;
				}

				// todo: this might cause commands to become stale and fill the pending command list
				if (PlayableStatus == EAvaPlayableStatus::Unloaded)
				{
					return false;
				}

				// Asset status must be visible to run the command.
				// If not visible, the components are not yet added to the world.
				if (PlayableStatus != EAvaPlayableStatus::Visible)
				{
					// Keep the command in the queue for next tick.
					bOutShouldDiscard = false;
					return false;
				}
			}
		}
		else
		{
			bOutShouldDiscard = true;
			return false;
		}
	}
	
	bOutShouldDiscard = true;
	return true;
}

void UAvaPlaybackServerTransition::Start()
{
	RegisterToPlayableTransitionEvent();

	// May fail if playables are not loaded yet. Playables are loaded
	// when the playback object has ticked at least one.
	MakePlayableTransition();

	bool bTransitionStarted = false;
	
	if (PlayableTransition)
	{
		LogDetailedTransitionInfo();

		// Todo: validate the level streaming playables are finished streaming the asset.
		// Otherwise, transition start must be queued on playable streaming events. 
		bTransitionStarted = PlayableTransition->Start();
	}

	if (!bTransitionStarted)
	{
		Stop();
	}
}

void UAvaPlaybackServerTransition::Stop()
{
	if (PlayableTransition)
	{
		PlayableTransition->Stop();
		PlayableTransition = nullptr;
	}

	for (const TWeakPtr<FAvaPlaybackInstance>& InstanceWeak : EnterPlaybackInstancesWeak)
	{
		if (const TSharedPtr<FAvaPlaybackInstance> Instance = InstanceWeak.Pin())
		{
			if (UAvaPlaybackGraph* Playback = Instance->GetPlayback())
			{
				Playback->OnPlayableCreated.RemoveAll(this);
			}
		}
	}
	
	UnregisterFromPlayableTransitionEvent();
	
	// Remove transition from server.
	if (const TSharedPtr<FAvaPlaybackServer> PlaybackServer = IAvaMediaModule::Get().GetPlaybackServerInternal())
	{
		if (!PlaybackServer->RemovePlaybackInstanceTransition(TransitionId))
		{
			UE_LOG(LogAvaPlaybackServer, Error,
				TEXT("Playback Transition {%s} Error: Was not found in server's active transitions. "), *GetPrettyTransitionInfo());
		}
	}
}

bool UAvaPlaybackServerTransition::IsRunning() const
{
	return PlayableTransition ? PlayableTransition->IsRunning() : false;
}

FString UAvaPlaybackServerTransition::GetPrettyTransitionInfo() const
{
	return FString::Printf(TEXT("Id:%s, Channel:%s, Client:%s"),
		*TransitionId.ToString(), *ChannelName.ToString(), *ClientName);
}

FString UAvaPlaybackServerTransition::GetBriefTransitionDescription() const
{
	auto MakeInstanceIdList = [](const TArray<TWeakPtr<FAvaPlaybackInstance>>& InPlaybackInstancesWeak) -> FString
	{
		FString InstanceIdList;
		for (const TWeakPtr<FAvaPlaybackInstance>& InstanceWeak : InPlaybackInstancesWeak)
		{
			if (const TSharedPtr<FAvaPlaybackInstance> Instance = InstanceWeak.Pin())
			{
				InstanceIdList += FString::Printf(TEXT("%s%s"), InstanceIdList.IsEmpty() ? TEXT("") : TEXT(", "), *Instance->GetInstanceId().ToString());
			}
		}
		return InstanceIdList.IsEmpty() ? TEXT("None") : InstanceIdList;
	};
	
	const FString EnterInstanceList = MakeInstanceIdList(EnterPlaybackInstancesWeak);
	const FString PlayingInstanceList = MakeInstanceIdList(PlayingPlaybackInstancesWeak);
	const FString ExitInstanceList = MakeInstanceIdList(ExitPlaybackInstancesWeak);

	return FString::Printf(TEXT("Enter Instance(s): [%s], Playing Instance(s): [%s], Exit Instance(s): [%s]."), *EnterInstanceList, *PlayingInstanceList, *ExitInstanceList);
}

TSharedPtr<FAvaPlaybackInstance> UAvaPlaybackServerTransition::FindInstanceForPlayable(const UAvaPlayable* InPlayable)
{
	using namespace UE::AvaPlaybackServerTransition;
	if (!InPlayable)
	{
		return nullptr;
	}

	TSharedPtr<FAvaPlaybackInstance> Instance =  Private::FindInstanceForPlayable(EnterPlaybackInstancesWeak, InPlayable);
	if (Instance)
	{
		return Instance;
	}
	
	Instance =  Private::FindInstanceForPlayable(PlayingPlaybackInstancesWeak, InPlayable);
	if (Instance)
	{
		return Instance;
	}

	Instance =  Private::FindInstanceForPlayable(ExitPlaybackInstancesWeak, InPlayable);
	if (Instance)
	{
		return Instance;
	}
	return nullptr;
}

void UAvaPlaybackServerTransition::OnTransitionEvent(UAvaPlayable* InPlayable, UAvaPlayableTransition* InTransition, EAvaPlayableTransitionEventFlags InTransitionFlags)
{
	using namespace UE::AvaPlaybackServerTransition::Private;
	
	// not this transition.
	if (InTransition != PlayableTransition || PlayableTransition == nullptr)
	{
		return;
	}

	const TSharedPtr<FAvaPlaybackServer> PlaybackServer = IAvaMediaModule::Get().GetPlaybackServerInternal();

	// Find the page player for this playable
	if (const TSharedPtr<FAvaPlaybackInstance> Instance = FindInstanceForPlayable(InPlayable))
	{
		// Relay the transition event back to the client
		if (PlaybackServer)
		{
			PlaybackServer->SendPlayableTransitionEvent(TransitionId, InPlayable->GetInstanceId(), InTransitionFlags, ChannelName, ClientName);
		}
		
		if (EnumHasAnyFlags(InTransitionFlags, EAvaPlayableTransitionEventFlags::StopPlayable))
		{
			// Validating that we are not removing an "enter" playable.
			if (PlayableTransition->IsEnterPlayable(InPlayable))
			{
				UE_LOG(LogAvaPlaybackServer, Error,
					TEXT("Playback Transition {%s} Error: An \"enter\" playable is being discarded for instance {%s}."),
					*GetPrettyTransitionInfo(), *GetPrettyPlaybackInstanceInfo(Instance.Get()));
			}

			// See UAvaRundownPagePlayer::Stop()
			const EAvaPlaybackStopOptions PlaybackStopOptions = bUnloadDiscardedInstances ?
				EAvaPlaybackStopOptions::Default | EAvaPlaybackStopOptions::Unload : EAvaPlaybackStopOptions::Default;	
			Instance->GetPlayback()->Stop(PlaybackStopOptions);
			
			if (bUnloadDiscardedInstances)
			{
				Instance->Unload();
				// Remove instance from the server.
				if (PlaybackServer)
				{
					if (!PlaybackServer->RemoveActivePlaybackInstance(Instance->GetInstanceId()))
					{
						UE_LOG(LogAvaPlaybackServer, Error,
							TEXT("Playback Transition {%s} Error: \"exit\" instance {%s} was not found in server's active instances. "),
							*GetPrettyTransitionInfo(), *GetPrettyPlaybackInstanceInfo(Instance.Get()));
					}
				}
			}
			else
			{
				Instance->Recycle();
			}
		}
	}

	if (EnumHasAnyFlags(InTransitionFlags, EAvaPlayableTransitionEventFlags::Finished))
	{
		if (PlaybackServer)
		{
			PlaybackServer->SendPlayableTransitionEvent(TransitionId, FGuid(), InTransitionFlags, ChannelName, ClientName);
		}
		
		Stop();
	}
}

void UAvaPlaybackServerTransition::OnPlayableCreated(UAvaPlaybackGraph* InPlayback, UAvaPlayable* InPlayable)
{
	if (UAvaPlayableGroup* PlayableGroup = InPlayable->GetPlayableGroup())
	{
		PlayableGroup->RegisterVisibilityConstraint(this);
	}
}

void UAvaPlaybackServerTransition::MakePlayableTransition()
{
	using namespace UE::AvaPlaybackServerTransition::Private;

	FAvaPlayableTransitionBuilder TransitionBuilder;

	auto AddInstancesToBuilder = [&TransitionBuilder, this](const TArray<TWeakPtr<FAvaPlaybackInstance>>& InPlaybackInstancesWeak, const TCHAR* InCategory, EAvaPlayableTransitionEntryRole InEntryRole)
	{
		using namespace UE::AvaPlaybackServerTransition::Private;
		int32 ArrayIndex = 0;
		for (const TWeakPtr<FAvaPlaybackInstance>& InstanceWeak : InPlaybackInstancesWeak)
		{
			if (const TSharedPtr<FAvaPlaybackInstance> Instance = InstanceWeak.Pin())
			{
				if (UAvaPlayable* Playable = GetPlayable(Instance.Get()))
				{
					const bool bPlayableAdded = TransitionBuilder.AddPlayable(Playable, InEntryRole);
					if (InEntryRole == EAvaPlayableTransitionEntryRole::Enter && bPlayableAdded)
					{
						TransitionBuilder.AddEnterPlayableValues(EnterValues.IsValidIndex(ArrayIndex) ? EnterValues[ArrayIndex] : nullptr);	
					}
				}
				else
				{
					// If this happens, likely the playable is not yet loaded.
					UE_LOG(LogAvaPlaybackServer, Error,
						TEXT("Playback Transition {%s} Error: Failed to retrieve \"%s\" playable for instance {%s}."),
						*GetPrettyTransitionInfo(), InCategory, *GetPrettyPlaybackInstanceInfo(Instance.Get()));
				}
			}
			++ArrayIndex;
		}
	};
 
	AddInstancesToBuilder(EnterPlaybackInstancesWeak, TEXT("Enter"), EAvaPlayableTransitionEntryRole::Enter);
	AddInstancesToBuilder(PlayingPlaybackInstancesWeak, TEXT("Playing"), EAvaPlayableTransitionEntryRole::Playing);
	AddInstancesToBuilder(ExitPlaybackInstancesWeak, TEXT("Exit"), EAvaPlayableTransitionEntryRole::Exit);
	PlayableTransition = TransitionBuilder.MakeTransition(this);

	if (PlayableTransition)
	{
		PlayableTransition->SetTransitionFlags(TransitionFlags);
	}
}

void UAvaPlaybackServerTransition::LogDetailedTransitionInfo() const
{
	UE_LOG(LogAvaPlaybackServer, Verbose, TEXT("Playback Transition {%s}:"), *GetPrettyTransitionInfo());

	auto LogInstances = [](const TArray<TWeakPtr<FAvaPlaybackInstance>>& InPlaybackInstancesWeak, const TCHAR* InCategory)
	{
		using namespace UE::AvaPlaybackServerTransition::Private;

		for (const TWeakPtr<FAvaPlaybackInstance>& InstanceWeak : InPlaybackInstancesWeak)
		{
			if (const TSharedPtr<FAvaPlaybackInstance> Instance = InstanceWeak.Pin())
			{
				UE_LOG(LogAvaPlaybackServer, Verbose, TEXT("- %s Instance: {%s}."), InCategory, *GetPrettyPlaybackInstanceInfo(Instance.Get()));
			}
		}
	};

	LogInstances(EnterPlaybackInstancesWeak, TEXT("Enter"));
	LogInstances(PlayingPlaybackInstancesWeak, TEXT("Playing"));
	LogInstances(ExitPlaybackInstancesWeak, TEXT("Exit"));
}

void UAvaPlaybackServerTransition::RegisterToPlayableTransitionEvent()
{
	UAvaPlayable::OnTransitionEvent().RemoveAll(this);
	UAvaPlayable::OnTransitionEvent().AddUObject(this, &UAvaPlaybackServerTransition::OnTransitionEvent);
}

void UAvaPlaybackServerTransition::UnregisterFromPlayableTransitionEvent() const
{
	UAvaPlayable::OnTransitionEvent().RemoveAll(this);
}

bool UAvaPlaybackServerTransition::AddPlaybackInstance(const TSharedPtr<FAvaPlaybackInstance>& InPlaybackInstance, TArray<TWeakPtr<FAvaPlaybackInstance>>& OutPlaybackInstancesWeak)
{
	if (!InPlaybackInstance)
	{
		return false;	
	}

	OutPlaybackInstancesWeak.Add(InPlaybackInstance);
	UpdateChannelName(InPlaybackInstance.Get());
	return true;
}

void UAvaPlaybackServerTransition::UpdateChannelName(const FAvaPlaybackInstance* InPlaybackInstance)
{
	if (ChannelName.IsNone())
	{
		ChannelName = InPlaybackInstance->GetChannelFName();
	}
	else
	{
		using namespace UE::AvaPlaybackServerTransition::Private;

		// Validate the channel is the same.
		if (ChannelName != InPlaybackInstance->GetChannelFName())
		{
			UE_LOG(LogAvaPlaybackServer, Error,
				TEXT("Playback Transition {%s}: Adding Playback Instance {%s} in a different channel than previous playback instance (\"%s\")."),
				*GetPrettyTransitionInfo(), *GetPrettyPlaybackInstanceInfo(InPlaybackInstance), *ChannelName.ToString());
		}
	}
}