// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/AvaPlayableGroupManager.h"

#include "Broadcast/AvaBroadcast.h"
#include "Broadcast/OutputDevices/AvaBroadcastRenderTargetMediaUtils.h"
#include "Framework/AvaGameInstance.h"
#include "Misc/CoreDelegates.h"
#include "Misc/TimeGuard.h"
#include "Playable/AvaPlayableGroup.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "AvaPlayableGroupManager"

UAvaPlayableGroup* UAvaPlayableGroupChannelManager::GetOrCreateSharedLevelGroup(bool bInIsRemoteProxy)
{
	if (UAvaPlayableGroup* ExistingPlayableGroup = bInIsRemoteProxy ? SharedRemoteProxyLevelGroupWeak.Get() : SharedLevelGroupWeak.Get())
	{
		return ExistingPlayableGroup;
	}
	
	UAvaPlayableGroup::FPlayableGroupCreationInfo PlayableGroupCreationInfo;
	PlayableGroupCreationInfo.PlayableGroupManager = GetPlayableGroupManager();
	PlayableGroupCreationInfo.ChannelName = ChannelName;
	PlayableGroupCreationInfo.bIsRemoteProxy = bInIsRemoteProxy;
	PlayableGroupCreationInfo.bIsSharedGroup = true;

	UAvaPlayableGroup* NewPlayableGroup = UAvaPlayableGroup::MakePlayableGroup(GetPlayableGroupManager(), PlayableGroupCreationInfo);

	// Keep track of the shared playable group.
	if (bInIsRemoteProxy)
	{
		SharedRemoteProxyLevelGroupWeak = NewPlayableGroup;
	}
	else
	{
		SharedLevelGroupWeak = NewPlayableGroup;
	}
	
	return NewPlayableGroup;
}

UAvaPlayableGroupManager* UAvaPlayableGroupChannelManager::GetPlayableGroupManager() const
{
	return Cast<UAvaPlayableGroupManager>(GetOuter());	
}

void UAvaPlayableGroupChannelManager::Shutdown()
{
	SharedRemoteProxyLevelGroupWeak.Reset();
	SharedLevelGroupWeak.Reset();
}

void UAvaPlayableGroupChannelManager::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UAvaPlayableGroupManager::Init()
{
	if (!UAvaGameInstance::GetOnEndPlay().IsBoundToObject(this))
	{
		UAvaGameInstance::GetOnEndPlay().AddUObject(this, &UAvaPlayableGroupManager::OnGameInstanceEndPlay);
	}
}

void UAvaPlayableGroupManager::Shutdown()
{
	for (TPair<FName, TObjectPtr<UAvaPlayableGroupChannelManager>>& Pair : ChannelManagers)
	{
		if (UAvaPlayableGroupChannelManager* const ChannelManager = Pair.Value)
		{
			ChannelManager->Shutdown();
		}
	}
	UAvaGameInstance::GetOnEndPlay().RemoveAll(this);
}

void UAvaPlayableGroupManager::Tick(double InDeltaSeconds)
{
	SCOPE_TIME_GUARD(TEXT("UAvaPlayableGroupManager::Tick"));
	UpdateLevelStreaming();
	TickTransitions(InDeltaSeconds);
}

UAvaPlayableGroupChannelManager* UAvaPlayableGroupManager::FindOrAddChannelManager(const FName& InChannelName)
{
	UAvaPlayableGroupChannelManager* ChannelManager = FindChannelManager(InChannelName);
	if (!ChannelManager)
	{
		ChannelManager = NewObject<UAvaPlayableGroupChannelManager>(this);
		ChannelManager->ChannelName = InChannelName;
		ChannelManagers.Add(InChannelName, ChannelManager);
	}
	return ChannelManager;
}

void UAvaPlayableGroupManager::RegisterForLevelStreamingUpdate(UAvaPlayableGroup* InPlayableGroup)
{
	if (ensure(!bIsUpdatingStreaming))
	{
		GroupsToUpdateStreaming.Add(InPlayableGroup);
	}
}

void UAvaPlayableGroupManager::UnregisterFromLevelStreamingUpdate(UAvaPlayableGroup* InPlayableGroup)
{
	if (!bIsUpdatingStreaming)
	{
		GroupsToUpdateStreaming.Remove(InPlayableGroup);
	}
}

void UAvaPlayableGroupManager::RegisterForTransitionTicking(UAvaPlayableGroup* InPlayableGroup)
{
	if (ensure(!bIsTickingTransitions))
	{
		GroupsToTickTransitions.Add(InPlayableGroup);
	}
}

void UAvaPlayableGroupManager::UnregisterFromTransitionTicking(UAvaPlayableGroup* InPlayableGroup)
{
	if (!bIsTickingTransitions)
	{
		GroupsToTickTransitions.Remove(InPlayableGroup);
	}
}

void UAvaPlayableGroupManager::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UAvaPlayableGroupManager::OnGameInstanceEndPlay(UAvaGameInstance* InGameInstance, FName InChannelName)
{
	FAvaBroadcastOutputChannel& Channel = UAvaBroadcast::Get().GetCurrentProfile().GetChannelMutable(InChannelName);
	if (!Channel.IsValidChannel())
	{
		return;
	}

	// In the current design, there is only one playable group active on a channel, i.e.
	// the graph of playables and groups must end on a root "playable group".
	// This is resolved by the playback graph(s) running on that channel.
	//
	// When we receive an EndPlay event from a game instance for a given channel, we need to
	// check if that is the current root playable group, and if it is, clear out the
	// channels association with it.
	//
	// If the channel is live, it will then start rendering the placeholder graphic, if
	// configured to do so, on the next slate tick.
	
	if (const UAvaPlayableGroup* PlayableGroup = Channel.GetLastActivePlayableGroup())
	{
		if (PlayableGroup->GetGameInstance() == InGameInstance)
		{
			Channel.UpdateRenderTarget(nullptr, nullptr);
			Channel.UpdateAudioDevice(FAudioDeviceHandle());
		}
	}

	// If the channel is not live, the placeholder graphic doesn't render
	// so we need to explicitly clear the channel here.
	if (Channel.GetState() != EAvaBroadcastChannelState::Live)
	{
		if (UTextureRenderTarget2D* const RenderTarget = Channel.GetCurrentRenderTarget(true))
		{
			UE::AvaBroadcastRenderTargetMediaUtils::ClearRenderTarget(RenderTarget);
		}
	}
}

void UAvaPlayableGroupManager::UpdateLevelStreaming()
{
	TGuardValue TransitionsTickGuard(bIsUpdatingStreaming, true);		
	for (TSet<TWeakObjectPtr<UAvaPlayableGroup>>::TIterator GroupIterator(GroupsToUpdateStreaming); GroupIterator; ++GroupIterator)
	{
		const UAvaPlayableGroup* GroupToUpdate = GroupIterator->Get();
		// We only update streaming if the group is not playing.
		if (GroupToUpdate && !GroupToUpdate->IsWorldPlaying())
		{
			// World may not be created yet.
			if (UWorld* PlayWorld = GroupToUpdate->GetPlayWorld())
			{
				// (This is normally updated by the game viewport client when the world is playing.)
				PlayWorld->UpdateLevelStreaming();

				// Check if still has streaming. If not, from the list.
				if (!PlayWorld->HasStreamingLevelsToConsider())
				{
					GroupIterator.RemoveCurrent();
				}
			}
		}
		else
		{
			GroupIterator.RemoveCurrent();
		}
	}
}

void UAvaPlayableGroupManager::TickTransitions(double InDeltaSeconds)
{
	TGuardValue TransitionsTickGuard(bIsTickingTransitions, true);
	for (TSet<TWeakObjectPtr<UAvaPlayableGroup>>::TIterator GroupIterator(GroupsToTickTransitions); GroupIterator; ++GroupIterator)
	{
		bool bHasTransitions = false;
		if (UAvaPlayableGroup* GroupToTick = GroupIterator->Get())
		{
			GroupToTick->TickTransitions(InDeltaSeconds);
			bHasTransitions = GroupToTick->HasTransitions();
		}

		// Group automatically deregister if stale or they don't have active transitions.
		if (!bHasTransitions)
		{
			GroupIterator.RemoveCurrent();
		}
	}
}

#undef LOCTEXT_NAMESPACE