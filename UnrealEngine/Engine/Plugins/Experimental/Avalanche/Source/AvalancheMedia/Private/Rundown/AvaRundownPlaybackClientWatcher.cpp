// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/AvaRundownPlaybackClientWatcher.h"

#include "Broadcast/AvaBroadcast.h"
#include "Playback/IAvaPlaybackClient.h"
#include "Rundown/AvaRundown.h"
#include "Rundown/AvaRundownPagePlayer.h"

FAvaRundownPlaybackClientWatcher::FAvaRundownPlaybackClientWatcher(UAvaRundown* InRundown)
	: Rundown(InRundown)
{
	using namespace UE::AvaPlaybackClient::Delegates;
	GetOnPlaybackStatusChanged().AddRaw(this, &FAvaRundownPlaybackClientWatcher::HandlePlaybackStatusChanged);
	
}
FAvaRundownPlaybackClientWatcher::~FAvaRundownPlaybackClientWatcher()
{
	using namespace UE::AvaPlaybackClient::Delegates;
	GetOnPlaybackStatusChanged().RemoveAll(this);
}

void FAvaRundownPlaybackClientWatcher::TryRestorePlaySubPage(int InPageId, const UE::AvaPlaybackClient::Delegates::FPlaybackStatusChangedArgs& InEventArgs) const
{
	// Restore page player and local playback proxies.
	// (Need to specify the InstanceId from the server for everything to match.)
	const FName ChannelFName(InEventArgs.ChannelName);

	// Ensure the specified channel exists.
	if (UAvaBroadcast::Get().GetChannelIndex(ChannelFName) == INDEX_NONE)
	{
		UE_LOG(LogAvaRundown, Error,
			TEXT("Received a playback object on channel \"%s\" which doesn't exist locally. Playback Server should be reset."),
			*InEventArgs.ChannelName);
		return;
	}
	
	const bool bIsPreview = UAvaBroadcast::Get().GetChannelType(ChannelFName) == EAvaBroadcastChannelType::Preview ? true : false;

	const FAvaRundownPage& PageToRestore = Rundown->GetPage(InPageId);

	const int32 SubPageIndex = PageToRestore.GetAssetPaths(Rundown).Find(InEventArgs.AssetPath);

	if (SubPageIndex == INDEX_NONE)
	{
		UE_LOG(LogAvaRundown, Error,
			TEXT("Asset mismatch (expected (any of): \"%s\", received: \"%s\") for restoring page %d. Playback Server should be reset."),
			*FString::JoinBy(PageToRestore.GetAssetPaths(Rundown), TEXT(","), [](const FSoftObjectPath& Path){ return Path.ToString();}),
			*InEventArgs.AssetPath.ToString(), InPageId);
		return;
	}

	if (!Rundown->RestorePlaySubPage(InPageId, SubPageIndex, InEventArgs.InstanceId, bIsPreview, ChannelFName))
	{
		UE_LOG(LogAvaRundown, Error, TEXT("Failed to restore page %d. Playback Server should be reset."), InPageId);
	}
}

void FAvaRundownPlaybackClientWatcher::HandlePlaybackStatusChanged(IAvaPlaybackClient& InPlaybackClient,
	const UE::AvaPlaybackClient::Delegates::FPlaybackStatusChangedArgs& InEventArgs)
{
	if (!Rundown)
	{
		return;
	}

	static const TArray<EAvaPlaybackStatus> RunningStates =
	{
		//EAvaPlaybackStatus::Starting, // Starting is not reliable, it may also mean "loading". FIXME.
		EAvaPlaybackStatus::Started
	};

	// Try to determine if a playback has started or stopped.
	const bool bWasRunning = IsAnyOf(InEventArgs.PrevStatus, RunningStates);
	const bool bIsRunning = IsAnyOf(InEventArgs.NewStatus, RunningStates);
	
	// If a playback instance is stopping, stop corresponding page (if any).
	if (bWasRunning && !bIsRunning)
	{
		for (UAvaRundownPagePlayer* PagePlayer : Rundown->PagePlayers)
		{
			// Search for a match with the event:				
			if (PagePlayer && PagePlayer->ChannelName == InEventArgs.ChannelName)
			{
				PagePlayer->ForEachInstancePlayer([&InEventArgs](UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
				{
					if (InInstancePlayer
						&& InInstancePlayer->SourceAssetPath == InEventArgs.AssetPath
						&& InInstancePlayer->GetPlaybackInstanceId() != InEventArgs.InstanceId)
					{
						InInstancePlayer->Stop();	
					}
				});

				// If we stopped all the instance players, stop the page (to broadcast events).
				if (!PagePlayer->IsPlaying())
				{
					PagePlayer->Stop();	
				}
			}
		}
		Rundown->RemoveStoppedPagePlayers();
	}

	// Note: execute this even if not on rising transition because it may be a user data update following the "GetUserData" request.
	if (bIsRunning)
	{
		// We need to figure out which page it is.
		const FString* UserData = InPlaybackClient.GetRemotePlaybackUserData(InEventArgs.InstanceId, InEventArgs.AssetPath, InEventArgs.ChannelName);

		// We haven't received the user data for this playback. So we request it.
		// This event will be received again with user data next time.
		if (!UserData)
		{
			InPlaybackClient.RequestPlayback(InEventArgs.InstanceId, InEventArgs.AssetPath, InEventArgs.ChannelName, EAvaPlaybackAction::GetUserData);
		}
		else
		{
			const int32 PageId = UAvaRundownPagePlayer::GetPageIdFromInstanceUserData(*UserData);
			if (PageId != FAvaRundownPage::InvalidPageId)
			{
				const UAvaRundownPagePlayer* PagePlayer = Rundown->FindPlayerForProgramPage(PageId);
				if (!PagePlayer || !PagePlayer->FindInstancePlayerByInstanceId(InEventArgs.InstanceId))
				{
					TryRestorePlaySubPage(PageId, InEventArgs);
				}
			}
		}
	}
}