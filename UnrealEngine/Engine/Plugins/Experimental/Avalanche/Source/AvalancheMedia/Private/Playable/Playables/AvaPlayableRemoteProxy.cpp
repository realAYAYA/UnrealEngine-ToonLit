// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/Playables/AvaPlayableRemoteProxy.h"

#include "Broadcast/AvaBroadcast.h"
#include "Framework/AvaSoftAssetPtr.h"
#include "IAvaMediaModule.h"
#include "Playable/AvaPlayableGroupManager.h"
#include "Playback/AvaPlaybackClientDelegates.h"
#include "Playback/IAvaPlaybackClient.h"

#define LOCTEXT_NAMESPACE "AvaPlayableRemoteProxy"

bool UAvaPlayableRemoteProxy::LoadAsset(const FAvaSoftAssetPtr& InSourceAsset, bool bInInitiallyVisible)
{
	if (!PlayableGroup)
	{
		return false;
	}
	
	SourceAssetPath = InSourceAsset.ToSoftObjectPath();

	IAvaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetPlaybackClient();
	
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		const FString ChannelName = PlayingChannelName;
		const FSoftObjectPath& AssetPath = InSourceAsset.ToSoftObjectPath();
		const TOptional<EAvaPlaybackStatus> RemoteStatusOpt = PlaybackClient.GetRemotePlaybackStatus(InstanceId, AssetPath, ChannelName);
		const EAvaPlaybackStatus RemoteStatus = RemoteStatusOpt.IsSet() ? RemoteStatusOpt.GetValue() : EAvaPlaybackStatus::Unknown;

		const bool bCanLoad = RemoteStatus == EAvaPlaybackStatus::Available
			|| RemoteStatus == EAvaPlaybackStatus::Unknown;
		const bool bIsLoaded = RemoteStatus == EAvaPlaybackStatus::Loading
			|| RemoteStatus == EAvaPlaybackStatus::Loaded
			|| RemoteStatus == EAvaPlaybackStatus::Starting
			|| RemoteStatus == EAvaPlaybackStatus::Started;

		if (bCanLoad && !bIsLoaded)
		{
			PlaybackClient.RequestPlayback(InstanceId, AssetPath, ChannelName, EAvaPlaybackAction::Load);
			PlaybackClient.RequestPlayback(InstanceId, AssetPath, ChannelName, EAvaPlaybackAction::SetUserData, UserData);
		}
	}
	return true;
}

bool UAvaPlayableRemoteProxy::UnloadAsset()
{
	IAvaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetPlaybackClient();
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		PlaybackClient.RequestPlayback(InstanceId, SourceAssetPath, PlayingChannelName, EAvaPlaybackAction::Unload);
	}
	return true;
}

namespace UE::AvaMedIaRemoteProxyPlayable::Private
{
	EAvaPlayableStatus GetPlayableStatus(EAvaPlaybackStatus InPlaybackStatus)
	{
		switch (InPlaybackStatus)
		{
		case EAvaPlaybackStatus::Unknown:
			return EAvaPlayableStatus::Unknown;
		case EAvaPlaybackStatus::Missing:
		case EAvaPlaybackStatus::Syncing:
		case EAvaPlaybackStatus::Available:
			return EAvaPlayableStatus::Unloaded;
		case EAvaPlaybackStatus::Loading:
			return EAvaPlayableStatus::Loading;
		case EAvaPlaybackStatus::Loaded:
			return EAvaPlayableStatus::Loaded;
		case EAvaPlaybackStatus::Starting:
			return EAvaPlayableStatus::Loaded;
		case EAvaPlaybackStatus::Started:
			return EAvaPlayableStatus::Visible;
		case EAvaPlaybackStatus::Stopping:
			return EAvaPlayableStatus::Loaded;
		case EAvaPlaybackStatus::Unloading:
			return EAvaPlayableStatus::Unloaded;
		case EAvaPlaybackStatus::Error:
		default:
			return EAvaPlayableStatus::Error;
		}
	}
}

EAvaPlayableStatus UAvaPlayableRemoteProxy::GetPlayableStatus() const
{
	IAvaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetPlaybackClient();
	const TArray<FString> OnlineServers = GetOnlineServerForChannel(PlayingChannelFName);
	
	for (const FString& Server : OnlineServers)
	{
		TOptional<EAvaPlaybackStatus> PlaybackStatus = PlaybackClient.GetRemotePlaybackStatus(InstanceId, SourceAssetPath, PlayingChannelName, Server);

		if (!PlaybackStatus.IsSet())
		{
			PlaybackClient.RequestPlayback(InstanceId, SourceAssetPath, PlayingChannelName, EAvaPlaybackAction::Status);
			PlaybackStatus = EAvaPlaybackStatus::Unknown;
		}

		// TODO: reconcile forked channels.
		return UE::AvaMedIaRemoteProxyPlayable::Private::GetPlayableStatus(PlaybackStatus.GetValue());
	}
	
	return EAvaPlayableStatus::Unknown;
}

IAvaSceneInterface* UAvaPlayableRemoteProxy::GetSceneInterface() const
{
	return nullptr;
}

EAvaPlayableCommandResult UAvaPlayableRemoteProxy::ExecuteAnimationCommand(EAvaPlaybackAnimAction InAnimAction, const FAvaPlaybackAnimPlaySettings& InAnimPlaySettings)
{
	IAvaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetPlaybackClient();
	
	// If an animation event was locally scheduled on a remote playable,
	// we need to propagate the event.
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		switch (InAnimAction)
		{
		case EAvaPlaybackAnimAction::None:
			break;
		case EAvaPlaybackAnimAction::Play:
		case EAvaPlaybackAnimAction::PreviewFrame:
			PlaybackClient.RequestAnimPlayback(InstanceId, SourceAssetPath, PlayingChannelName, InAnimPlaySettings);
			break;

		case EAvaPlaybackAnimAction::Continue:
		case EAvaPlaybackAnimAction::Stop:
		case EAvaPlaybackAnimAction::CameraCut:
			PlaybackClient.RequestAnimAction(InstanceId, SourceAssetPath, PlayingChannelName, InAnimPlaySettings.AnimationName.ToString(), InAnimAction);
			break;

		default:
			UE_LOG(LogAvaPlayable, Warning,
				TEXT("Animation command action \"%s\" for asset \"%s\" on channel \"%s\" is not implemented."),
				*StaticEnum<EAvaPlaybackAnimAction>()->GetValueAsString(InAnimAction),
				*SourceAssetPath.ToString(), *PlayingChannelName);
			break;
		}
	}
	return EAvaPlayableCommandResult::Executed;
}

EAvaPlayableCommandResult UAvaPlayableRemoteProxy::UpdateRemoteControlCommand(const TSharedRef<FAvaPlayableRemoteControlValues>& InRemoteControlValues)
{
	IAvaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetPlaybackClient();
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		PlaybackClient.RequestRemoteControlUpdate(InstanceId, SourceAssetPath, PlayingChannelName, *InRemoteControlValues);
	}
	return EAvaPlayableCommandResult::Executed;
}

bool UAvaPlayableRemoteProxy::ApplyCamera()
{
	return false;
}

void UAvaPlayableRemoteProxy::SetUserData(const FString& InUserData)
{
	if (UserData != InUserData)
	{
		IAvaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetPlaybackClient();
		if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
		{
			PlaybackClient.RequestPlayback(InstanceId, GetSourceAssetPath(), PlayingChannelName, EAvaPlaybackAction::SetUserData, InUserData);
		}
	}
	
	Super::SetUserData(InUserData);
}

bool UAvaPlayableRemoteProxy::InitPlayable(const FPlayableCreationInfo& InPlayableInfo)
{
	// We keep track of the channel this playable is part of.
	PlayingChannelFName = InPlayableInfo.ChannelName;
	PlayingChannelName = InPlayableInfo.ChannelName.ToString();
	
	constexpr bool bIsRemoteProxy = true;

	// Remote playables have proxy playable groups imitating the same setup as local ones.
	switch (InPlayableInfo.SourceAsset.GetAssetType())
	{
	case EMotionDesignAssetType::World:
		PlayableGroup = InPlayableInfo.PlayableGroupManager->GetOrCreateSharedLevelGroup(InPlayableInfo.ChannelName, bIsRemoteProxy);
		break;
	default:
		UE_LOG(LogAvaPlayable, Error, TEXT("Asset \"%s\" is an unsupported type."), *InPlayableInfo.SourceAsset.ToSoftObjectPath().ToString());
		break;
	}
	
	const bool bInitSucceeded = Super::InitPlayable(InPlayableInfo);
	if (bInitSucceeded)
	{
		RegisterClientEventHandlers();
	}
	return bInitSucceeded;
}

void UAvaPlayableRemoteProxy::OnPlay()
{
	IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	if (!AvaMediaModule.IsPlaybackClientStarted())
	{
		return;
	}
	
	IAvaPlaybackClient& Client = AvaMediaModule.GetPlaybackClient();
	
	if (Client.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		const FString ChannelName = PlayingChannelName;
		const TOptional<EAvaPlaybackStatus> RemoteStatusOpt = Client.GetRemotePlaybackStatus(InstanceId, SourceAssetPath, ChannelName);
		const EAvaPlaybackStatus RemoteStatus = RemoteStatusOpt.IsSet() ? RemoteStatusOpt.GetValue() : EAvaPlaybackStatus::Unknown;
		// TODO: rework this logic.
		if (RemoteStatus == EAvaPlaybackStatus::Available
			|| RemoteStatus == EAvaPlaybackStatus::Loading || RemoteStatus == EAvaPlaybackStatus::Loaded
			|| RemoteStatus == EAvaPlaybackStatus::Unknown || RemoteStatus == EAvaPlaybackStatus::Stopping
			|| RemoteStatus == EAvaPlaybackStatus::Unloading)
		{
			Client.RequestPlayback(InstanceId, SourceAssetPath, ChannelName, EAvaPlaybackAction::Start);
		}
	}
}

void UAvaPlayableRemoteProxy::OnEndPlay()
{
	IAvaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetPlaybackClient();
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		PlaybackClient.RequestPlayback(InstanceId, SourceAssetPath, PlayingChannelName, EAvaPlaybackAction::Stop);
	}
}

void UAvaPlayableRemoteProxy::BeginDestroy()
{
	UnregisterClientEventHandlers();
	Super::BeginDestroy();
}

void UAvaPlayableRemoteProxy::RegisterClientEventHandlers()
{
	using namespace UE::AvaPlaybackClient::Delegates;
	GetOnPlaybackSequenceEvent().RemoveAll(this);
	GetOnPlaybackSequenceEvent().AddUObject(this, &UAvaPlayableRemoteProxy::HandleAvaPlaybackSequenceEvent);
}

void UAvaPlayableRemoteProxy::UnregisterClientEventHandlers() const
{
	using namespace UE::AvaPlaybackClient::Delegates;
	GetOnPlaybackSequenceEvent().RemoveAll(this);
}

TArray<FString> UAvaPlayableRemoteProxy::GetOnlineServerForChannel(const FName& InChannelName)
{
	const FAvaBroadcastOutputChannel& Channel = UAvaBroadcast::Get().GetCurrentProfile().GetChannel(InChannelName);
	const TArray<UMediaOutput*>& Outputs = Channel.GetMediaOutputs();
	TArray<FString> OnlineServers;
	OnlineServers.Reserve(Outputs.Num());
	
	for (const UMediaOutput* Output : Outputs)
	{
		if (Channel.IsMediaOutputRemote(Output) && Channel.GetMediaOutputState(Output) != EAvaBroadcastOutputState::Offline)
		{
			OnlineServers.AddUnique(Channel.GetMediaOutputServerName(Output));
		}
	}
	return OnlineServers;
}

void UAvaPlayableRemoteProxy::HandleAvaPlaybackSequenceEvent(IAvaPlaybackClient& InPlaybackClient,
	const UE::AvaPlaybackClient::Delegates::FPlaybackSequenceEventArgs& InEventArgs)
{
	if (InEventArgs.InstanceId == InstanceId && InEventArgs.ChannelName == PlayingChannelName)
	{
		const FName SequenceName(InEventArgs.SequenceName);
		OnSequenceEventDelegate.Broadcast(this, SequenceName, InEventArgs.EventType);
	}
}

#undef LOCTEXT_NAMESPACE
