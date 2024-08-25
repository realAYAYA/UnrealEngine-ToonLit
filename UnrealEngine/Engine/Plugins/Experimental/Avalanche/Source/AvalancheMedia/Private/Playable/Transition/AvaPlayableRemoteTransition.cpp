// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/Transition/AvaPlayableRemoteTransition.h"

#include "IAvaMediaModule.h"
#include "Playable/AvaPlayable.h"
#include "Playable/Transition/AvaPlayableTransitionPrivate.h"
#include "Playback/AvaPlaybackClientDelegates.h"
#include "Playback/AvaPlaybackUtils.h"
#include "Playback/IAvaPlaybackClient.h"

UAvaPlayableRemoteTransition::~UAvaPlayableRemoteTransition()
{
	UnregisterFromPlaybackClientDelegates();
}

bool UAvaPlayableRemoteTransition::Start()
{
	IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	
	if (!AvaMediaModule.IsPlaybackClientStarted())
	{
		TransitionId.Invalidate();
		return false;
	}

	if (!Super::Start())
	{
		return false;
	}

	TransitionId = FGuid::NewGuid();
	
	using namespace UE::AvaPlayableTransition::Private;
	TArray<FGuid> EnterInstanceIds = GetInstanceIds(EnterPlayablesWeak);
	TArray<FGuid> PlayingInstanceIds = GetInstanceIds(PlayingPlayablesWeak);
	TArray<FGuid> ExitInstanceIds = GetInstanceIds(ExitPlayablesWeak);
	TArray<FAvaPlayableRemoteControlValues> EnterValues;
	EnterValues.Reserve(EnterPlayableValues.Num());
	for (const TSharedPtr<FAvaPlayableRemoteControlValues>& Values : EnterPlayableValues)
	{
		EnterValues.Add(Values.IsValid() ? *Values : FAvaPlayableRemoteControlValues::GetDefaultEmpty());
	}
	
	IAvaPlaybackClient& PlaybackClient = AvaMediaModule.GetPlaybackClient();
	PlaybackClient.RequestPlayableTransitionStart(TransitionId, MoveTemp(EnterInstanceIds), MoveTemp(PlayingInstanceIds), MoveTemp(ExitInstanceIds), MoveTemp(EnterValues), ChannelName, TransitionFlags);
	RegisterToPlaybackClientDelegates();	// to get the transition events from the server side.

	using namespace UE::AvaPlayback::Utils;
	UE_LOG(LogAvaPlayable, Verbose, TEXT("%s Remote Playable Transition \"%s\" Id:\"%s\" starting."), *GetBriefFrameInfo(), *GetFullName(), *TransitionId.ToString());
	return true;
}

void UAvaPlayableRemoteTransition::Stop()
{
	if (TransitionId.IsValid())
	{
		IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	
		if (AvaMediaModule.IsPlaybackClientStarted())
		{
			AvaMediaModule.GetPlaybackClient().RequestPlayableTransitionStop(TransitionId, ChannelName);
		}
	}

	Super::Stop();
}

void UAvaPlayableRemoteTransition::RegisterToPlaybackClientDelegates()
{
	UE::AvaPlaybackClient::Delegates::GetOnPlaybackTransitionEvent().RemoveAll(this);
	UE::AvaPlaybackClient::Delegates::GetOnPlaybackTransitionEvent().AddUObject(this, &UAvaPlayableRemoteTransition::HandlePlaybackTransitionEvent);
}

void UAvaPlayableRemoteTransition::UnregisterFromPlaybackClientDelegates() const
{
	UE::AvaPlaybackClient::Delegates::GetOnPlaybackTransitionEvent().RemoveAll(this);
}

bool UAvaPlayableRemoteTransition::IsRunning() const
{
	return TransitionId.IsValid();	
}

void UAvaPlayableRemoteTransition::HandlePlaybackTransitionEvent(IAvaPlaybackClient& InPlaybackClient,
	const UE::AvaPlaybackClient::Delegates::FPlaybackTransitionEventArgs& InArgs)
{
	if (InArgs.TransitionId != TransitionId)
	{
		return;
	}

	if (InArgs.InstanceId.IsValid())
	{
		if (UAvaPlayable* Playable = FindPlayable(InArgs.InstanceId))
		{
			// Relay locally through playable event.
			UAvaPlayable::OnTransitionEvent().Broadcast(Playable, this, InArgs.EventFlags);
		}
		else
		{
			UE_LOG(LogAvaPlayable, Error,
				TEXT("Remote Playable Transition \"%s\" doesn't have playable instance Id \"%s\"."),
				*TransitionId.ToString(), *InArgs.InstanceId.ToString());
		}
	}
	else
	{
		UAvaPlayable::OnTransitionEvent().Broadcast(nullptr, this, InArgs.EventFlags);
	}

	if (EnumHasAnyFlags(InArgs.EventFlags, EAvaPlayableTransitionEventFlags::Finished))
	{
		using namespace UE::AvaPlayback::Utils;
		UE_LOG(LogAvaPlayable, Verbose, TEXT("%s Remote Playable Transition \"%s\" Id:\"%s\" ended."), *GetBriefFrameInfo(), *GetFullName(), *TransitionId.ToString());
	}
}

