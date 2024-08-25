// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/Transition/AvaRundownPageTransition.h"

#include "Playable/Transition/AvaPlayableTransition.h"
#include "Playback/AvaPlaybackManager.h"
#include "Playback/AvaPlaybackUtils.h"
#include "Rundown/AvaRundown.h"
#include "Rundown/AvaRundownPagePlayer.h"

namespace UE::AvaMedia::Rundown::PageTransition::Private
{
	UAvaPlayable* GetPlayable(const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
	{
		return InInstancePlayer ? InInstancePlayer->GetFirstPlayable() : nullptr;
	}
	
	UAvaPlayable* GetPlayable(const UAvaRundownPagePlayer* InPagePlayer, int32 InIndex)
	{
		return InPagePlayer ? GetPlayable(InPagePlayer->GetInstancePlayer(InIndex)) : nullptr;
	}

	UAvaPlaybackGraph* GetPlayback(const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
	{
		return InInstancePlayer ? InInstancePlayer->Playback : nullptr; 
	}
	
	UAvaPlaybackGraph* GetPlayback(const UAvaRundownPagePlayer* InPagePlayer, int32 InIndex)
	{
		return InPagePlayer ? GetPlayback(InPagePlayer->GetInstancePlayer(InIndex)) : nullptr;
	}

	TSharedPtr<FAvaPlayableRemoteControlValues> GetRemoteControlValues(const UAvaRundownPagePlayer* InPagePlayer)
	{
		if (UAvaRundown* Rundown = InPagePlayer->GetRundown())
		{
			const FAvaRundownPage& Page = Rundown->GetPage(InPagePlayer->PageId);
			if (Page.IsValidPage())
			{
				return MakeShared<FAvaPlayableRemoteControlValues>(Page.GetRemoteControlValues());
			}
		}
		return nullptr;
	}

	FString GetPrettyPageInfo(const UAvaRundownPagePlayer* InPagePlayer)
	{
		FString Info;
		Info += FString(TEXT("Channel: ")) + InPagePlayer->ChannelName;
		InPagePlayer->ForEachInstancePlayer([&Info](const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
		{
			Info += FString(TEXT(", Instance Id: ")) + InInstancePlayer->GetPlaybackInstanceId().ToString();
			Info += FString(TEXT(", Asset: ")) + InInstancePlayer->SourceAssetPath.ToString();
		});
		return Info;
	}
	
	UAvaRundownPlaybackInstancePlayer* FindInstancePlayerForPlayable(const UAvaPlayable* InPlayable, const TArray<TWeakObjectPtr<UAvaRundownPagePlayer>>& InPagePlayersWeak)
	{
		for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : InPagePlayersWeak)
		{
			if (const UAvaRundownPagePlayer* PagePlayer = PlayerWeak.Get())
			{
				if (UAvaRundownPlaybackInstancePlayer* InstancePlayer = PagePlayer->FindInstancePlayerForPlayable(InPlayable))
				{
					return InstancePlayer;
				}
			}
		}
		return nullptr;
	}

	UAvaRundownPagePlayer* FindPagePlayerForPlayable(const UAvaPlayable* InPlayable, const TArray<TWeakObjectPtr<UAvaRundownPagePlayer>>& InPagePlayersWeak)
	{
		for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : InPagePlayersWeak)
		{
			UAvaRundownPagePlayer* PagePlayer = PlayerWeak.Get();
			if (PagePlayer && PagePlayer->HasPlayable(InPlayable))
			{
				return PagePlayer;
			}
		}
		return nullptr;
	}
}

bool UAvaRundownPageTransition::AddEnterPage(UAvaRundownPagePlayer* InPagePlayer)
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;

	if (!InPagePlayer)
	{
		return false;	
	}

	// Multi-page constraint: Prevent pages on the same layer
	for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : EnterPlayersWeak)
	{
		if (const UAvaRundownPagePlayer* Player = PlayerWeak.Get())
		{
			bool bHasLayer = false;
			InPagePlayer->ForEachInstancePlayer([this, &bHasLayer](const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
			{
				if (CachedTransitionLayers.Contains(InInstancePlayer->TransitionLayer.TagId))
				{
					bHasLayer = true;
				}
			});
			
			if (bHasLayer)
			{
				UE_LOG(LogAvaRundown, Error,
						TEXT("Page Transition \"%s\" Error: page %d can't be played with page %d because they are on the same layer."),
						*GetFullName(), InPagePlayer->PageId, Player->PageId);
				return false;
			}
		}
	}

	RegisterEnterPagePlayerEvents(InPagePlayer);
	
	return AddPagePlayer(InPagePlayer, EnterPlayersWeak);
}

bool UAvaRundownPageTransition::AddPlayingPage(UAvaRundownPagePlayer* InPagePlayer)
{
	return AddPagePlayer(InPagePlayer, PlayingPlayersWeak);
}

bool UAvaRundownPageTransition::AddExitPage(UAvaRundownPagePlayer* InPagePlayer)
{
	return AddPagePlayer(InPagePlayer, ExitPlayersWeak);
}

bool UAvaRundownPageTransition::IsVisibilityConstrained(const UAvaPlayable* InPlayable) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;
	bool bAllPlayablesLoaded = true;
	bool bIsPlayableInThisTransition = false;
	
	for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : EnterPlayersWeak)
	{
		if (const UAvaRundownPagePlayer* Player = PlayerWeak.Get())
		{
			Player->ForEachInstancePlayer([InPlayable, &bIsPlayableInThisTransition, &bAllPlayablesLoaded](const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
			{
				if (const UAvaPlayable* Playable = GetPlayable(InInstancePlayer))
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
			});
		}
	}
	
	return bIsPlayableInThisTransition && !bAllPlayablesLoaded;
}

bool UAvaRundownPageTransition::CanStart(bool& bOutShouldDiscard) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;
	for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : EnterPlayersWeak)
	{
		if (const UAvaRundownPagePlayer* Player = PlayerWeak.Get())
		{
			const int32 NumInstancePlayers = Player->GetNumInstancePlayers();
			for (int32 InstancePlayerIndex = 0; InstancePlayerIndex < NumInstancePlayers; ++InstancePlayerIndex)
			{
				if (const UAvaPlayable* Playable = GetPlayable(Player, InstancePlayerIndex))
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

					// Asset status must be visible to run the command locally.
					// If not visible, the components are not yet added to the world.
					// For remote proxies, we can run the command immediately, it will wait for the asset to be visible on the server instead. 
					if (!Playable->IsRemoteProxy() && PlayableStatus != EAvaPlayableStatus::Visible)
					{
						// Keep the command in the queue for next tick.
						bOutShouldDiscard = false;
						return false;
					}
				}
				else
				{
					// Playables not created - Keep the command in the queue for next tick.
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

void UAvaRundownPageTransition::Start()
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

void UAvaRundownPageTransition::Stop()
{
	if (PlayableTransition)
	{
		PlayableTransition->Stop();
		PlayableTransition = nullptr;
	}

	for (TWeakObjectPtr<UAvaRundownPagePlayer>& PagePlayerWeak : EnterPlayersWeak)
	{
		if (UAvaRundownPagePlayer* PagePlayer = PagePlayerWeak.Get())
		{
			PagePlayer->InstancesBypassingTransition.Reset();
			UnregisterEnterPagePlayerEvents(PagePlayer);
		}
	}

	UnregisterFromPlayableTransitionEvent();

	if (UAvaRundown* Rundown = GetRundown())
	{
		Rundown->RemovePageTransition(this);
		Rundown->RemoveStoppedPagePlayers();
	}
	else
	{
		UE_LOG(LogAvaRundown, Error, TEXT("Page Transition \"%s\" Failed to remove transition: No rundown specified."), *GetFullName());
	}
}

bool UAvaRundownPageTransition::IsRunning() const
{
	return 	PlayableTransition ? PlayableTransition->IsRunning() : false;
}

bool UAvaRundownPageTransition::HasEnterPagesWithNoTransitionLogic() const
{
	const UAvaRundown* Rundown = GetRundown();
	if (!Rundown)
	{
		return false;
	}
	
	for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : EnterPlayersWeak)
	{
		if (const UAvaRundownPagePlayer* Player = PlayerWeak.Get())
		{
			// Don't rely on instance players at this point. Assets may not be loaded.
			const FAvaRundownPage& Page = Rundown->GetPage(Player->PageId);
			if (Page.IsValidPage() && !Page.HasTransitionLogic(Rundown))
			{
				return true;
			}
		}
	}
	return false;
}

bool UAvaRundownPageTransition::HasPagePlayer(const UAvaRundownPagePlayer* InPagePlayer) const
{
	return EnterPlayersWeak.Contains(InPagePlayer) || PlayingPlayersWeak.Contains(InPagePlayer) || ExitPlayersWeak.Contains(InPagePlayer);
}

bool UAvaRundownPageTransition::ContainsTransitionLayer(const FAvaTagId& InTagId) const
{
	return CachedTransitionLayers.Contains(InTagId);
}

UAvaRundown* UAvaRundownPageTransition::GetRundown() const
{
	return Cast<UAvaRundown>(GetOuter());
}

UAvaRundownPlaybackInstancePlayer* UAvaRundownPageTransition::FindInstancePlayerForPlayable(const UAvaPlayable* InPlayable) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition;
	if (!InPlayable)
	{
		return nullptr;
	}
	
	if (UAvaRundownPlaybackInstancePlayer* FoundInstancePlayer = Private::FindInstancePlayerForPlayable(InPlayable, EnterPlayersWeak))
	{
		return FoundInstancePlayer;
	}
	if (UAvaRundownPlaybackInstancePlayer* FoundInstancePlayer = Private::FindInstancePlayerForPlayable(InPlayable, PlayingPlayersWeak))
	{
		return FoundInstancePlayer;
	}
	if (UAvaRundownPlaybackInstancePlayer* FoundInstancePlayer = Private::FindInstancePlayerForPlayable(InPlayable, ExitPlayersWeak))
	{
		return FoundInstancePlayer;
	}
	return nullptr;
}

UAvaRundownPagePlayer* UAvaRundownPageTransition::FindPagePlayerForPlayable(const UAvaPlayable* InPlayable) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition;
	if (UAvaRundownPagePlayer* FoundPagePlayer = Private::FindPagePlayerForPlayable(InPlayable, EnterPlayersWeak))
	{
		return FoundPagePlayer;
	}
	if (UAvaRundownPagePlayer* FoundPagePlayer = Private::FindPagePlayerForPlayable(InPlayable, PlayingPlayersWeak))
	{
		return FoundPagePlayer;
	}
	if (UAvaRundownPagePlayer* FoundPagePlayer = Private::FindPagePlayerForPlayable(InPlayable, ExitPlayersWeak))
	{
		return FoundPagePlayer;
	}
	return nullptr;
}

void UAvaRundownPageTransition::OnTransitionEvent(UAvaPlayable* InPlayable, UAvaPlayableTransition* InTransition, EAvaPlayableTransitionEventFlags InTransitionFlags)
{
	using namespace UE::AvaPlayback::Utils;

	// not this transition.
	if (InTransition != PlayableTransition || PlayableTransition == nullptr)
	{
		return;
	}

	// Find the page player for this playable
	UAvaRundownPlaybackInstancePlayer* InstancePlayer = FindInstancePlayerForPlayable(InPlayable);
	UAvaRundownPagePlayer* PagePlayer = InstancePlayer ? InstancePlayer->GetPagePlayer() : nullptr;

	if (InstancePlayer && EnumHasAnyFlags(InTransitionFlags, EAvaPlayableTransitionEventFlags::MarkPlayableDiscard))
	{
		UE_LOG(LogAvaRundown, Verbose, TEXT("%s Instance Player Marked for Discard: Id:%s, Asset:\"%s\""),
			*GetBriefFrameInfo(), *InstancePlayer->GetPlaybackInstanceId().ToString(), *InstancePlayer->SourceAssetPath.ToString());
	}
	
	if (InstancePlayer && EnumHasAnyFlags(InTransitionFlags, EAvaPlayableTransitionEventFlags::StopPlayable))
	{
		// Validating that we are not removing an "enter" playable.
		if (PlayableTransition->IsEnterPlayable(InPlayable))
		{
			UE_LOG(LogAvaRundown, Error,
				TEXT("Page Transition \"%s\" Error: An \"enter\" playable is being discarded for page %d."),
				*GetFullName(), (PagePlayer ? PagePlayer->PageId : -1));
		}

		UE_LOG(LogAvaRundown, Verbose, TEXT("%s Stopping Instance Player: Id:%s, Asset:\"%s\""),
			*GetBriefFrameInfo(), *InstancePlayer->GetPlaybackInstanceId().ToString(), *InstancePlayer->SourceAssetPath.ToString());

		// With combo-templates, page players can be partially stopped.
		InstancePlayer->Stop();

		// Check if the page player is still playing (i.e. if all instance players have been stopped)
		if (PagePlayer && !PagePlayer->IsPlaying())
		{
			UE_LOG(LogAvaRundown, Verbose, TEXT("%s Stopping Page Player: PageId:%d"), *GetBriefFrameInfo(), PagePlayer->PageId);

			// Stop the whole page player and propagate page events.
			PagePlayer->Stop();

			if (UAvaRundown* Rundown = PagePlayer->GetRundown())
			{
				Rundown->RemoveStoppedPagePlayers();
			}
			else
			{
				UE_LOG(LogAvaRundown, Error, TEXT("Page Transition \"%s\" failed to remove stopped players: No rundown specified."), *GetFullName());
			}
		}
	}
	
	if (EnumHasAnyFlags(InTransitionFlags, EAvaPlayableTransitionEventFlags::Finished))
	{
		using namespace UE::AvaPlayback::Utils;
		UE_LOG(LogAvaRundown, Verbose, TEXT("%s Finishing Page Transition: %s"), *GetBriefFrameInfo(), *GetBriefTransitionDescription());

		Stop();
	}
}

void UAvaRundownPageTransition::OnPlayableCreated(UAvaPlaybackGraph* InPlayback, UAvaPlayable* InPlayable)
{
	if (UAvaPlayableGroup* PlayableGroup = InPlayable->GetPlayableGroup())
	{
		PlayableGroup->RegisterVisibilityConstraint(this);
	}
}

void UAvaRundownPageTransition::AddPlayersToBuilder(
	FAvaPlayableTransitionBuilder& InOutBuilder, const TArray<TWeakObjectPtr<UAvaRundownPagePlayer>>& InPlayersWeak, const TCHAR* InCategory, EAvaPlayableTransitionEntryRole InEntryRole) const
{
	for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : InPlayersWeak)
	{
		if (UAvaRundownPagePlayer* Player = PlayerWeak.Get())
		{
			AddPlayablesToBuilder(InOutBuilder, Player, InCategory, InEntryRole);
			
			// Bypassed instances are only for current transition.
			// Todo: Ideally, it would be stored in the transition itself to avoid the clean up.
			Player->InstancesBypassingTransition.Reset();
		}
	}
}

void UAvaRundownPageTransition::AddPlayablesToBuilder(FAvaPlayableTransitionBuilder& InOutBuilder, const UAvaRundownPagePlayer* InPlayer, const TCHAR* InCategory, EAvaPlayableTransitionEntryRole InEntryRole) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;

	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InPlayer->InstancePlayers)
	{
		EAvaPlayableTransitionEntryRole EntryRole = InEntryRole;

		// -- Special Transition Logic --
		if (InPlayer->InstancesBypassingTransition.Contains(InstancePlayer->GetPlaybackInstanceId()))
		{
			if (EntryRole != EAvaPlayableTransitionEntryRole::Enter)
			{
				continue;	// Completely skipping if not an "enter" instance.
			}

			// Enter instances excluded should still be part as playing pages.
			EntryRole = EAvaPlayableTransitionEntryRole::Playing;
		}
	
		if (UAvaPlayable* Playable = GetPlayable(InstancePlayer))
		{
			const bool bPlayableAdded = InOutBuilder.AddPlayable(Playable, EntryRole);
			if (EntryRole == EAvaPlayableTransitionEntryRole::Enter && bPlayableAdded)
			{
				InOutBuilder.AddEnterPlayableValues(GetRemoteControlValues(InPlayer));
			}
		}
		else
		{
			// If this happens, likely the playable is not yet loaded.
			UE_LOG(LogAvaRundown, Error,
				TEXT("Page Transition \"%s\" Error: Failed to retrieve \"%s\" playable for page %d."),
				*GetFullName(), InCategory, InPlayer->PageId);
		}
	}
}

void UAvaRundownPageTransition::MakePlayableTransition()
{
	FAvaPlayableTransitionBuilder TransitionBuilder;
	AddPlayersToBuilder(TransitionBuilder, EnterPlayersWeak, TEXT("Enter"), EAvaPlayableTransitionEntryRole::Enter);
	AddPlayersToBuilder(TransitionBuilder, PlayingPlayersWeak, TEXT("Playing"), EAvaPlayableTransitionEntryRole::Playing);
	AddPlayersToBuilder(TransitionBuilder, ExitPlayersWeak, TEXT("Exit"), EAvaPlayableTransitionEntryRole::Exit);
	PlayableTransition = TransitionBuilder.MakeTransition(this);

	// For non-TL enter pages, we need to kick out the playing pages too. 
	if (PlayableTransition && HasEnterPagesWithNoTransitionLogic())
	{
		PlayableTransition->SetTransitionFlags(EAvaPlayableTransitionFlags::TreatPlayingAsExiting);
	}
}

void UAvaRundownPageTransition::LogDetailedTransitionInfo() const
{
	if (!UE_LOG_ACTIVE(LogAvaRundown, Verbose))
	{
		return;
	}
	
	using namespace UE::AvaPlayback::Utils;
	UE_LOG(LogAvaRundown, Verbose, TEXT("%s Starting Page Transition \"%s\":"), *GetBriefFrameInfo(), *GetFullName());
	
	auto LogPlayers = [this](const TArray<TWeakObjectPtr<UAvaRundownPagePlayer>>& InPlayersWeak, const TCHAR* InCategory)
	{
		using namespace UE::AvaMedia::Rundown::PageTransition::Private;
		for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : InPlayersWeak)
		{
			if (const UAvaRundownPagePlayer* Player = PlayerWeak.Get())
			{
				UE_LOG(LogAvaRundown, Verbose, TEXT("- %s Page: %d, %s."), InCategory, Player->PageId, *GetPrettyPageInfo(Player));
			}
		}
	};

	LogPlayers(EnterPlayersWeak, TEXT("Enter"));
	LogPlayers(PlayingPlayersWeak, TEXT("Playing"));
	LogPlayers(ExitPlayersWeak, TEXT("Exit"));
}

FString UAvaRundownPageTransition::GetBriefTransitionDescription() const
{
	auto MakePageIdList = [](const TArray<TWeakObjectPtr<UAvaRundownPagePlayer>>& InPlayersWeak) -> FString
	{
		FString PageIdList;
		for (const TWeakObjectPtr<UAvaRundownPagePlayer>& PlayerWeak : InPlayersWeak)
		{
			if (const UAvaRundownPagePlayer* Player = PlayerWeak.Get())
			{
				PageIdList += FString::Printf(TEXT("%s%d"), PageIdList.IsEmpty() ? TEXT("") : TEXT(", "), Player->PageId);
			}
		}
		return PageIdList.IsEmpty() ? TEXT("None") : PageIdList;
	};

	const FString EnterPageList = MakePageIdList(EnterPlayersWeak);
	const FString PlayingPageList = MakePageIdList(PlayingPlayersWeak);
	const FString ExitPageList = MakePageIdList(ExitPlayersWeak);
	return FString::Printf(TEXT("Page Transition \"%s\": Enter Page(s): [%s], Playing Page(s): [%s], Exit Page(s): [%s]."),
		*GetFullName(), *EnterPageList, *PlayingPageList, *ExitPageList);
}

void UAvaRundownPageTransition::RegisterToPlayableTransitionEvent()
{
	UAvaPlayable::OnTransitionEvent().RemoveAll(this);
	UAvaPlayable::OnTransitionEvent().AddUObject(this, &UAvaRundownPageTransition::OnTransitionEvent);
}

void UAvaRundownPageTransition::UnregisterFromPlayableTransitionEvent() const
{
	UAvaPlayable::OnTransitionEvent().RemoveAll(this);
}

void UAvaRundownPageTransition::RegisterEnterPagePlayerEvents(UAvaRundownPagePlayer* InPagePlayer)
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;

	if (!InPagePlayer)
	{
		return;
	}
	
	for (const UAvaRundownPlaybackInstancePlayer* InstancePlayer : InPagePlayer->InstancePlayers)
	{
		CachedTransitionLayers.Add(InstancePlayer->TransitionLayer.TagId);

		// Register this transition as a visibility constraint for the playable group.
		if (const UAvaPlayable* Playable = GetPlayable(InstancePlayer))
		{
			if (UAvaPlayableGroup* PlayableGroup = Playable->GetPlayableGroup())
			{
				PlayableGroup->RegisterVisibilityConstraint(this);
			}
		}
		else if (UAvaPlaybackGraph* Playback = GetPlayback(InstancePlayer))
		{
			// If the playable is not created yet, register to the creation event.
			Playback->OnPlayableCreated.AddUObject(this, &UAvaRundownPageTransition::OnPlayableCreated);
		}
	}
}

void UAvaRundownPageTransition::UnregisterEnterPagePlayerEvents(UAvaRundownPagePlayer* InPagePlayer)
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;
	
	if (!InPagePlayer)
	{
		return;
	}
	
	InPagePlayer->ForEachInstancePlayer([this](const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
	{
		if (UAvaPlaybackGraph* Playback = GetPlayback(InInstancePlayer))
		{
			Playback->OnPlayableCreated.RemoveAll(this);
			Playback->ForEachPlayable([this](const UAvaPlayable* InPlayable)
			{
				if (UAvaPlayableGroup* PlayableGroup = InPlayable->GetPlayableGroup())
				{
					PlayableGroup->UnregisterVisibilityConstraint(this);
				}
			});
		}
	});
}

bool UAvaRundownPageTransition::AddPagePlayer(UAvaRundownPagePlayer* InPagePlayer, TArray<TWeakObjectPtr<UAvaRundownPagePlayer>>& OutPagePlayersWeak)
{
	if (!InPagePlayer)
	{
		return false;	
	}

	OutPagePlayersWeak.Add(InPagePlayer);

	UpdateChannelName(InPagePlayer);

	return true;
}

void UAvaRundownPageTransition::UpdateChannelName(const UAvaRundownPagePlayer* InPagePlayer)
{
	check(InPagePlayer);
	
	if (ChannelName.IsNone())
	{
		ChannelName = InPagePlayer->ChannelFName;
	}
	else
	{
		using namespace UE::AvaMedia::Rundown::PageTransition::Private;

		// Validate the channel is the same.
		if (ChannelName != InPagePlayer->ChannelFName)
		{
			UE_LOG(LogAvaRundown, Error, TEXT("Page Transition \"%s\": Adding Page: %d, {%s} in a different channel than previous pages (\"%s\")."),
				*GetFullName(), InPagePlayer->PageId, *GetPrettyPageInfo(InPagePlayer), *ChannelName.ToString());
		}
	}
}

