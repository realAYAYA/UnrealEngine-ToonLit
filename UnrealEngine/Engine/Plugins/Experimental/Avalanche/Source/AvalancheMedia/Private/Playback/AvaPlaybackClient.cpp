// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvaPlaybackClient.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AvaMediaMessageUtils.h"
#include "AvaMediaModule.h"
#include "AvaMediaSettings.h"
#include "Broadcast/AvaBroadcast.h"
#include "Broadcast/OutputDevices/AvaBroadcastOutputUtils.h"
#include "IAvaModule.h"
#include "IMediaIOCoreModule.h"
#include "MediaOutput.h"
#include "MessageEndpointBuilder.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaPlaybackClient, Log, All);

namespace UE::AvaPlaybackClient::Private
{
	template<typename InEnumType>
	FString EnumToString(InEnumType InValue)
	{
		return StaticEnum<InEnumType>()->GetNameStringByValue(static_cast<int64>(InValue));
	}
}

FAvaPlaybackClient::FAvaPlaybackClient(FAvaMediaModule* InParentModule)
	: ParentModule(InParentModule)
{
	UPackage::PreSavePackageWithContextEvent.AddRaw(this, &FAvaPlaybackClient::OnPreSavePackage);
	UPackage::PackageSavedWithContextEvent.AddRaw(this, &FAvaPlaybackClient::OnPackageSaved);
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnAssetRemoved().AddRaw(this, &FAvaPlaybackClient::OnAssetRemoved);
	}
}

FAvaPlaybackClient::~FAvaPlaybackClient()
{
	if (BroadcastChangedHandle.IsValid())
	{
		UAvaBroadcast::Get().RemoveChangeListener(BroadcastChangedHandle);
	}

	FAvaBroadcastOutputChannel::GetOnChannelChanged().RemoveAll(this);
	
	UPackage::PreSavePackageWithContextEvent.RemoveAll(this);
	UPackage::PackageSavedWithContextEvent.RemoveAll(this);
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnAssetRemoved().RemoveAll(this);
	}
	
	FTSTicker::GetCoreTicker().RemoveTicker(PingTickDelegateHandle);
	FCoreDelegates::OnEndFrame.RemoveAll(this);
	FMessageEndpoint::SafeRelease(MessageEndpoint);

	for (IConsoleObject* ConsoleCmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ConsoleCmd);
	}
	ConsoleCommands.Empty();

#if WITH_EDITOR
	if (UObjectInitialized())
	{
		UAvaMediaSettings& AvaMediaSettings = UAvaMediaSettings::GetMutable();
		AvaMediaSettings.OnSettingChanged().RemoveAll(this);
	}
#endif
}

void FAvaPlaybackClient::Init()
{
	ComputerName = FPlatformProcess::ComputerName();
	ProjectContentPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	ProcessId = FPlatformProcess::GetCurrentProcessId();
	
	RegisterCommands();

	// Create our end point.
	MessageEndpoint = FMessageEndpoint::Builder("AvaPlaybackClient")
		.Handling<FAvaPlaybackPong>(this, &FAvaPlaybackClient::HandlePlaybackPongMessage)
		.Handling<FAvaPlaybackLog>(this, &FAvaPlaybackClient::HandlePlaybackLogMessage)
		.Handling<FAvaPlaybackUpdateServerUserData>(this, &FAvaPlaybackClient::HandleUpdateServerUserData)
		.Handling<FAvaPlaybackStatStatus>(this, &FAvaPlaybackClient::HandleStatStatus)
		.Handling<FAvaBroadcastDeviceProviderDataList>(this, &FAvaPlaybackClient::HandleDeviceProviderDataListMessage)
		.Handling<FAvaBroadcastStatus>(this, &FAvaPlaybackClient::HandleBroadcastStatusMessage)
		.Handling<FAvaPlaybackAssetStatus>(this, &FAvaPlaybackClient::HandlePlaybackAssetStatusMessage)
		.Handling<FAvaPlaybackStatus>(this, &FAvaPlaybackClient::HandlePlaybackStatusMessage)
		.Handling<FAvaPlaybackStatuses>(this, &FAvaPlaybackClient::HandlePlaybackStatusesMessage)
		.Handling<FAvaPlaybackSequenceEvent>(this, &FAvaPlaybackClient::HandlePlaybackSequenceEventMessage)
		.Handling<FAvaPlaybackTransitionEvent>(this, &FAvaPlaybackClient::HandlePlaybackTransitionEventMessage);
	
	if (MessageEndpoint.IsValid())
	{
		PingTickDelegate = FTickerDelegate::CreateRaw(this, &FAvaPlaybackClient::HandlePingTicker);
		PingTickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(PingTickDelegate, UAvaMediaSettings::Get().PingInterval);
		FCoreDelegates::OnEndFrame.AddRaw(this, &FAvaPlaybackClient::Tick);
		
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("Motion Design Playback Client \"%s\" Started."), *ComputerName);
	}

	BroadcastChangedHandle = UAvaBroadcast::Get().AddChangeListener(
		FOnAvaBroadcastChanged::FDelegate::CreateRaw(this, &FAvaPlaybackClient::OnBroadcastChanged));

	FAvaBroadcastOutputChannel::GetOnChannelChanged().AddRaw(this, &FAvaPlaybackClient::OnChannelChanged);
	
#if WITH_EDITOR
	UAvaMediaSettings& AvaMediaSettings = UAvaMediaSettings::GetMutable();
	AvaMediaSettings.OnSettingChanged().AddRaw(this, &FAvaPlaybackClient::OnAvaMediaSettingsChanged);
#endif

	ApplyAvaMediaSettings();
}

int32 FAvaPlaybackClient::GetNumConnectedServers() const
{
	return Servers.Num();
}

TArray<FString> FAvaPlaybackClient::GetServerNames() const
{
	TArray<FString> ServerNames;
	ServerNames.Empty(Servers.Num());
	ForAllServers([&ServerNames](const FServerInfo& InServerInfo)
	{
		ServerNames.Add(InServerInfo.ServerName);
	});
	return ServerNames;
}

FMessageAddress FAvaPlaybackClient::GetServerAddress(const FString& InServerName) const
{
	if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		return ServerInfo->Address;
	}
	FMessageAddress InvalidAddress;
	InvalidAddress.Invalidate();
	return InvalidAddress;
}

FString FAvaPlaybackClient::GetServerProjectContentPath(const FString& InServerName) const
{
	if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		return ServerInfo->ProjectContentPath;
	}
	return FString();
}

uint32 FAvaPlaybackClient::GetServerProcessId(const FString& InServerName) const
{
	if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		return ServerInfo->ProcessId;
	}
	return 0;
}

bool FAvaPlaybackClient::HasServerUserData(const FString& InServerName, const FString& InKey) const
{
	if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		return ServerInfo->UserDataEntries.Contains(InKey);
	}
	return false;
}

const FString& FAvaPlaybackClient::GetServerUserData(const FString& InServerName, const FString& InKey) const
{
	if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		if (const FString* Data = ServerInfo->UserDataEntries.Find(InKey))
		{
			return *Data;
		}
	}
	static FString EmptyData;
	return EmptyData;
}

bool FAvaPlaybackClient::HasUserData(const FString& InKey) const
{
	return UserDataEntries.Contains(InKey);
}

const FString& FAvaPlaybackClient::GetUserData(const FString& InKey) const
{
	if (const FString* Data = UserDataEntries.Find(InKey))
	{
		return *Data;
	}
	static FString EmptyString;
	return EmptyString;
}

void FAvaPlaybackClient::SetUserData(const FString& InKey, const FString& InData)
{
	UserDataEntries.Add(InKey, InData);
	SendUserDataUpdate(AllServerAddresses);
}

void FAvaPlaybackClient::RemoveUserData(const FString& InKey)
{
	UserDataEntries.Remove(InKey);
	SendUserDataUpdate(AllServerAddresses);
}

void FAvaPlaybackClient::BroadcastStatCommand(const FString& InCommand, bool bInBroadcastLocalState)
{
	SendStatCommand(InCommand, bInBroadcastLocalState, AllServerAddresses);
}

void FAvaPlaybackClient::RequestPlaybackAssetStatus(const FSoftObjectPath& InAssetPath, const FString& InChannelOrServerName, bool bInForceRefresh)
{
	if (!InAssetPath.IsValid())
	{
		UE_LOG(LogAvaPlaybackClient, Warning, TEXT("Invalid asset path for Playback Asset Status Request."));
		return;
	}
	
	TArray<FString> ServerNames = Servers.Contains(InChannelOrServerName) ? TArray<FString>{InChannelOrServerName} : GetServerNamesForChannel(FName(InChannelOrServerName));
	
	for (const FString& ServerName : ServerNames)
	{
		RequestPlaybackAssetStatusForServer(InAssetPath, ServerName, bInForceRefresh);
	}
}

void FAvaPlaybackClient::RequestPlayback(const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath, const FString& InChannelName,
	EAvaPlaybackAction InAction, const FString& InArguments)
{
	// Possibly TMP. Maybe find a generic way to deal with pending requests.
	// Status requests can be spammed every frame by the UI so we block them
	// here to avoid spamming the message bus and servers.
	if (InAction == EAvaPlaybackAction::Status)
	{
		const FDateTime CurrentTime = FDateTime::UtcNow();
		const FString RequestKey = GetPlaybackStatusRequestKey(InInstanceId, InAssetPath, InChannelName);
		const FDateTime* PendingRequest = PendingPlaybackStatusRequests.Find(RequestKey);
		// Check if we already have a pending request and that it hasn't expired.
		if (PendingRequest && CurrentTime < *PendingRequest)
		{
			return;
		}

		// Keep track of the request and the time it was made so we have a timeout.
		const FDateTime ExpirationTime = CurrentTime + FTimespan::FromSeconds(UAvaMediaSettings::Get().ClientPendingStatusRequestTimeout);
		PendingPlaybackStatusRequests.Add(RequestKey, ExpirationTime);
	}

	// Special case where the command is sent on all the assets and channels on all servers.
	if (InChannelName.IsEmpty() || InAssetPath.IsNull())
	{
		FAvaPlaybackRequest* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackRequest>();
		Request->Commands.Reserve(1);
		Request->Commands.Add({InInstanceId, InAssetPath, InChannelName, InAction, InArguments});
		SendRequest(Request, AllServerAddresses);
		return;
	}
	
	if (InAssetPath.IsValid())
	{
		// Update the status of the playback for each server that we made a request to.
		TArray<FString> ServerNames = GetServerNamesForChannel(FName(InChannelName));
		for (const FString& ServerName : ServerNames)
		{
			if (FServerInfo* ServerInfo = GetServerInfo(ServerName))
			{
				// The command will be sent in a batch on the next Tick.
				ServerInfo->PendingPlaybackCommands.Add({InInstanceId, InAssetPath, InChannelName, InAction, InArguments});
				
				switch (InAction)
				{
				case EAvaPlaybackAction::None:
				case EAvaPlaybackAction::Status:
				case EAvaPlaybackAction::GetUserData:
				case EAvaPlaybackAction::SetUserData:
					break;
				case EAvaPlaybackAction::Load:
					ServerInfo->SetInstanceStatus(InChannelName, InInstanceId, InAssetPath, EAvaPlaybackStatus::Loading);
					break;
				case EAvaPlaybackAction::Start:
					ServerInfo->SetInstanceStatus(InChannelName, InInstanceId, InAssetPath, EAvaPlaybackStatus::Starting);
					break;
				case EAvaPlaybackAction::Stop:
					ServerInfo->SetInstanceStatus(InChannelName, InInstanceId, InAssetPath, EAvaPlaybackStatus::Stopping);
					break;
				case EAvaPlaybackAction::Unload:
					ServerInfo->SetInstanceStatus(InChannelName, InInstanceId, InAssetPath, EAvaPlaybackStatus::Unloading);
					break;
				}
			}
		}
	}
	else
	{
		// We will wait for feedback from server, if it managed to play the asset correctly.
		UE_LOG(LogAvaPlaybackClient, Warning,
			   TEXT("Playback request \"%s\" on channel \"%s\" with no asset specified. Unable to update playback status."),
			   *UE::AvaPlaybackClient::Private::EnumToString(InAction),
			   *InChannelName);
	}
}

void FAvaPlaybackClient::RequestAnimPlayback(const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath, const FString& InChannelName,
												  const FAvaPlaybackAnimPlaySettings& InAnimSettings)
{
	FAvaPlaybackAnimPlaybackRequest* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackAnimPlaybackRequest>();
	Request->InstanceId = InInstanceId;
	Request->AssetPath = InAssetPath;
	Request->ChannelName = InChannelName;
	Request->AnimPlaySettings.Add(InAnimSettings);

	if (!InAssetPath.IsValid())
	{
		UE_LOG(LogAvaPlaybackClient, Warning, TEXT("Animation \"%s\" request with invalid asset path on channel \"%s\"."),
			*InAnimSettings.AnimationName.ToString(), *InChannelName);
	}
		
	SendRequest(Request, GetServerAddressesForChannel(InChannelName));
}

void FAvaPlaybackClient::RequestAnimAction(const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath, const FString& InChannelName,
												  const FString& InAnimationName, EAvaPlaybackAnimAction InAction)
{
	FAvaPlaybackAnimPlaybackRequest* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackAnimPlaybackRequest>();
	Request->InstanceId = InInstanceId;
	Request->AssetPath = InAssetPath;
	Request->ChannelName = InChannelName;
	Request->AnimActionInfos.Add({InAnimationName, InAction});

	if (!InAssetPath.IsValid())
	{
		UE_LOG(LogAvaPlaybackClient, Warning, TEXT("Animation \"%s\" %s request with invalid asset path on channel \"%s\"."),
			*InAnimationName, *StaticEnum<EAvaPlaybackAnimAction>()->GetValueAsString(InAction), *InChannelName);
	}
	
	SendRequest(Request, GetServerAddressesForChannel(InChannelName));
}

void FAvaPlaybackClient::RequestRemoteControlUpdate(const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath,
														 const FString& InChannelName,
														 const FAvaPlayableRemoteControlValues& InRemoteControlValues)
{
	FAvaPlaybackRemoteControlUpdateRequest* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackRemoteControlUpdateRequest>();
	Request->InstanceId = InInstanceId;
	Request->AssetPath = InAssetPath;
	Request->ChannelName = InChannelName;
	Request->RemoteControlValues = InRemoteControlValues;

	if (!InAssetPath.IsValid())
	{
		UE_LOG(LogAvaPlaybackClient, Warning, TEXT("Remote Control Values Update request with invalid asset path on channel \"%s\"."), *InChannelName);
	}
	
	SendRequest(Request, GetServerAddressesForChannel(InChannelName));
}

void FAvaPlaybackClient::RequestPlayableTransitionStart(const FGuid& InTransitionId, TArray<FGuid>&& InEnterInstanceIds, TArray<FGuid>&& InPlayingInstanceIds, TArray<FGuid>&& InExitInstanceIds, TArray<FAvaPlayableRemoteControlValues>&& InEnterValues, const FName& InChannelName, EAvaPlayableTransitionFlags InTransitionFlags)
{
	FAvaPlaybackTransitionStartRequest* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackTransitionStartRequest>();
	Request->TransitionId = InTransitionId;
	Request->ChannelName = InChannelName.ToString();
	Request->EnterInstanceIds = MoveTemp(InEnterInstanceIds);
	Request->PlayingInstanceIds = MoveTemp(InPlayingInstanceIds);
	Request->ExitInstanceIds = MoveTemp(InExitInstanceIds);
	Request->EnterValues = MoveTemp(InEnterValues);
	Request->bUnloadDiscardedInstances = !UAvaMediaSettings::Get().bKeepPagesLoaded;
	Request->SetTransitionFlags(InTransitionFlags);
	SendRequest(Request, GetServerAddressesForChannel(InChannelName));
}

void FAvaPlaybackClient::RequestPlayableTransitionStop(const FGuid& InTransitionId, const FName& InChannelName)
{
	FAvaPlaybackTransitionStopRequest* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackTransitionStopRequest>();
	Request->TransitionId = InTransitionId;
	Request->ChannelName = InChannelName.ToString();
	Request->bUnloadDiscardedInstances = !UAvaMediaSettings::Get().bKeepPagesLoaded;
	SendRequest(Request, GetServerAddressesForChannel(InChannelName));
}

void FAvaPlaybackClient::RequestBroadcast(const FString& InProfile, const FName& InChannel,
											   const TArray<UMediaOutput*>& InRemoteMediaOutputs,
											   EAvaBroadcastAction InAction)
{
	FAvaBroadcastRequest* Request = FMessageEndpoint::MakeMessage<FAvaBroadcastRequest>();
	Request->Profile = InProfile;
	Request->Channel = InChannel.ToString();
	Request->Action = InAction;

	bool bNeedMediaOutputs = (InAction == EAvaBroadcastAction::Start || InAction == EAvaBroadcastAction::UpdateConfig);
	if (Request->Channel.IsEmpty())
	{
		// If no channel is specified, no need to send outputs. 
		bNeedMediaOutputs = false;
		
		if (InAction == EAvaBroadcastAction::UpdateConfig)
		{
			UE_LOG(LogAvaPlaybackClient, Error, TEXT("Invalid Broadcast Request: UpdateConfig requires a channel to be specified."));
			return;
		}
	}
	
	if (bNeedMediaOutputs)
	{
		// For now, we send the media output objects in the request.
		uint32 TotalOutputDataSize = 0;
		for (UMediaOutput* const MediaOutput : InRemoteMediaOutputs)
		{
			if (IsValid(MediaOutput))
			{
				FAvaBroadcastOutputData MediaOutputData = UE::AvaBroadcastOutputUtils::CreateMediaOutputData(MediaOutput);
				
				// Also propagate output info. Necessary to send the Guid and Server.
				MediaOutputData.OutputInfo
					= UAvaBroadcast::Get().GetCurrentProfile().GetChannel(InChannel).GetMediaOutputInfo(MediaOutput);
				
				if (!MediaOutputData.OutputInfo.IsValid())
				{
					UE_LOG(LogAvaPlaybackClient, Warning, TEXT("MediaOutput information is not valid for channel \"%s\"."),
						   *InChannel.ToString());
				}
				TotalOutputDataSize += MediaOutputData.SerializedData.Num();
				Request->MediaOutputs.Add(MoveTemp(MediaOutputData));
			}
		}
		
		// Adding a warning here, if we hit this warning, it may be necessary
		// to send the data through some other transport.
		// Data size is about 1k per output. It is unlikely we will hit this limit.
		const uint32 SafeMessageSizeLimit = UE::AvaMediaMessageUtils::GetSafeMessageSizeLimit();	
		if (TotalOutputDataSize > SafeMessageSizeLimit)
		{
			UE_LOG(LogAvaPlaybackClient, Warning,
				TEXT("The broadcast request (DataSize: %d) is larger that the safe message size limit (%d)."),
				TotalOutputDataSize, SafeMessageSizeLimit);
		}
	}

	const TArray<FMessageAddress> ServerAddressesForChannel = GetServerAddressesForChannel(InChannel);

	// Also update the channel settings.
	if (InAction == EAvaBroadcastAction::Start || InAction == EAvaBroadcastAction::UpdateConfig)
	{
		SendBroadcastChannelSettingsUpdate(ServerAddressesForChannel, UAvaBroadcast::Get().GetCurrentProfile().GetChannel(InChannel));
	}

	// Send to server(s) that have output for this channel.
	SendRequest(Request, ServerAddressesForChannel);
}

bool FAvaPlaybackClient::IsMediaOutputRemoteFallback(const UMediaOutput* InMediaOutput)
{
	const FString DeviceName = UE::AvaBroadcastOutputUtils::GetDeviceName(InMediaOutput);
	if (!DeviceName.IsEmpty())
	{
		// See if it begins with any of the remote server names that are connected.
		// See FAvaBroadcastDeviceProviderData::ApplyServerName. All MediaOutput from replicated Device Provider
		// Have the server name in the beginning of the device name.
		for (const TPair<FString, TSharedPtr<FServerInfo>>& Server : Servers)
		{
			if (DeviceName.StartsWith(Server.Key))
			{
				return true;
			}
		}
	}

	// We may fall in this case if the device is from a remote server that is offline
	// right now. So we still want to double check if the device is a local one.

	// Search in the list of device providers for the local server only.
	const FName DeviceProviderName = UE::AvaBroadcastOutputUtils::GetDeviceProviderName(InMediaOutput);
	if (!DeviceProviderName.IsNone() && !DeviceName.IsEmpty())
	{
		// First check if we have a wrapper for this provider. This means
		// we have some remote devices currently online on that provider.
		const FAvaBroadcastDeviceProviderWrapper* DeviceProviderWrapper =
			ParentModule->GetDeviceProviderProxyManager().GetDeviceProviderWrapper(DeviceProviderName);

		IMediaIOCoreDeviceProvider* LocalDeviceProvider;

		if (DeviceProviderWrapper && DeviceProviderWrapper->HasLocalProvider())
		{
			LocalDeviceProvider = DeviceProviderWrapper->GetLocalProvider();
		}
		else
		{
			// If we don't have a wrapper, then directly fetch the concrete local device provider.
			LocalDeviceProvider = IMediaIOCoreModule::Get().GetDeviceProvider(DeviceProviderName);
		}

		if (LocalDeviceProvider)
		{
			const FName DeviceFName(DeviceName);
			TArray<FMediaIODevice> LocalDevices = LocalDeviceProvider->GetDevices();
			for (const FMediaIODevice& LocalDevice : LocalDevices)
			{
				if (DeviceFName == LocalDevice.DeviceName)
				{
					return false; // found as local device, so not remote.
				}
			}
			// If we didn't find the device in local provider (and it has a local provider),
			// then we can safely consider it a remote device, but it's server is currently offline.
			return true;
		}
	}

	// By default the device will be local. But this is not a good default.
	return false;
}

EAvaBroadcastIssueSeverity FAvaPlaybackClient::GetMediaOutputIssueSeverity(
	const FString& InServerName, const FString& InChannelName, const FGuid& InOutputGuid) const
{
	if (const FBroadcastChannelInfo* ChannelInfo = GetChannelInfo(InServerName, InChannelName))
	{
		if (const FAvaBroadcastOutputStatus* MediaOutputStatus = ChannelInfo->MediaOutputStatuses.Find(InOutputGuid))
		{
			return MediaOutputStatus->MediaIssueSeverity;
		}
	}
	return EAvaBroadcastIssueSeverity::None;
}

const TArray<FString>& FAvaPlaybackClient::GetMediaOutputIssueMessages(
	const FString& InServerName, const FString& InChannelName, const FGuid& InOutputGuid) const
{
	if (const FBroadcastChannelInfo* ChannelInfo = GetChannelInfo(InServerName, InChannelName))
	{
		if (const FAvaBroadcastOutputStatus* MediaOutputStatus = ChannelInfo->MediaOutputStatuses.Find(InOutputGuid))
		{
			return MediaOutputStatus->MediaIssueMessages;
		}
	}
	static const TArray<FString> EmptyStringArray;
	return EmptyStringArray;
}

EAvaBroadcastOutputState FAvaPlaybackClient::GetMediaOutputState(
	const FString& InServerName, const FString& InChannelName, const FGuid& InOutputGuid) const
{
	if (const FBroadcastChannelInfo* ChannelInfo = GetChannelInfo(InServerName, InChannelName))
	{
		if (const FAvaBroadcastOutputStatus* MediaOutputStatus = ChannelInfo->MediaOutputStatuses.Find(InOutputGuid))
		{
			return MediaOutputStatus->MediaOutputState;
		}
		else
		{
			// If the server is connected but doesn't have a status yet, we consider it "idle".
			return EAvaBroadcastOutputState::Idle;
		}
	}
	// If we couldn't find the channel state in the list of servers,
	// then consider this output as offline.
	return EAvaBroadcastOutputState::Offline;
}

// Note: same logic as GetServerNamesForChannel.
bool FAvaPlaybackClient::HasAnyServerOnlineForChannel(const FName& InChannelName) const
{
	if (InChannelName.IsNone())
	{
		return false;
	}
	
	const FAvaBroadcastOutputChannel& Channel = UAvaBroadcast::Get().GetCurrentProfile().GetChannel(InChannelName);
	if (!Channel.IsValidChannel())
	{
		return false;
	}

	const TArray<UMediaOutput*>& RemoteOutputs = Channel.GetRemoteMediaOutputs();
	
	for (const UMediaOutput* RemoteOutput : RemoteOutputs)
	{
		const FAvaBroadcastMediaOutputInfo& OutputInfo = Channel.GetMediaOutputInfo(RemoteOutput);
		if (OutputInfo.IsValid())
		{
			if (Servers.Contains(OutputInfo.ServerName))
			{
				return true;
			}
		}
		else
		{
			UE_LOG(LogAvaPlaybackClient, Warning, TEXT("MediaOutputInfo invalid for channel \"%s\"."), *InChannelName.ToString());
			
			// Try to find the server name from the device name.
			const FString ServerName = GetServerNameForMediaOutputFallback(RemoteOutput);
			if (!ServerName.IsEmpty())
			{
				return true;
			}
		}
	}
	
	return false;
}

TOptional<EAvaPlaybackStatus> FAvaPlaybackClient::GetRemotePlaybackStatus(
	const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath, const FString& InChannelName, const FString& InServerName) const
{
	TOptional<EAvaPlaybackStatus> PlaybackStatus;

	if (!InServerName.IsEmpty())
	{
		PlaybackStatus = GetPlaybackStatusForServer(InServerName, InChannelName, InInstanceId, InAssetPath);
	}
	else
	{
		// Because of "forked" channels, we could actually have more than one server playing this channel.
		TArray<FString> ServerNames = GetServerNamesForChannel(FName(InChannelName));

		// In this case, we return the first one found. It is not accurate though.
		// Todo: Ideally, we should retire this code path.
		for (const FString& ServerName : ServerNames)
		{
			PlaybackStatus = GetPlaybackStatusForServer(ServerName, InChannelName, InInstanceId, InAssetPath);
			if (PlaybackStatus.IsSet())
			{
				break;
			}
		}
	}

	return PlaybackStatus;
}

const FString* FAvaPlaybackClient::GetRemotePlaybackUserData(
	const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath, const FString& InChannelName, const FString& InServerName) const
{
	if (!InServerName.IsEmpty())
	{
		return GetPlaybackUserDataForServer(InServerName, InChannelName, InInstanceId, InAssetPath);
	}

	// Because of "forked" channels, we could actually have more than one server playing this channel.
	TArray<FString> ServerNames = GetServerNamesForChannel(FName(InChannelName));

	// In this case, we return the first one found. It is not accurate though.
	// Todo: Ideally, we should retire this code path.
	for (const FString& ServerName : ServerNames)
	{
		if (const FString* FoundUserData = GetPlaybackUserDataForServer(ServerName, InChannelName, InInstanceId, InAssetPath))
		{
			return FoundUserData;
		}
	}

	return nullptr;
}

TOptional<EAvaPlaybackAssetStatus> FAvaPlaybackClient::GetRemotePlaybackAssetStatus(
	const FSoftObjectPath& InAssetPath, const FString& InServerName) const
{
	if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		return ServerInfo->GetPlaybackAssetStatus(InAssetPath);
	}
	return TOptional<EAvaPlaybackAssetStatus>();
}

TOptional<EAvaPlaybackStatus> FAvaPlaybackClient::GetPlaybackStatusForServer(
	const FString& InServerName, const FString& InChannelName, const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath) const
{
	if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		return ServerInfo->GetInstanceStatus(InChannelName, InInstanceId, InAssetPath);
	}
	return TOptional<EAvaPlaybackStatus>();
}

const FString* FAvaPlaybackClient::GetPlaybackUserDataForServer(
	const FString& InServerName, const FString& InChannelName, const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath) const
{
	if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		return ServerInfo->GetInstanceUserData(InChannelName, InInstanceId, InAssetPath);
	}
	return nullptr;
}

void FAvaPlaybackClient::RequestPlaybackAssetStatusForServer(const FSoftObjectPath& InAssetPath, const FString& InServerName, bool bInForceRefresh)
{
	FServerInfo* ServerInfo = GetServerInfo(InServerName);
	if (!ServerInfo)
	{
		UE_LOG(LogAvaPlaybackClient, Warning, TEXT("Specified server \"%s\" not found for Playback Asset Status Request."), *InServerName);
		return;
	}
	
	// Status requests can be spammed every frame by the UI so we block them
	// here to avoid spamming the message bus and servers.
	const FDateTime CurrentTime = FDateTime::UtcNow();
	if (const FPendingPlaybackAssetStatusRequest* PendingRequest = ServerInfo->PendingPlaybackAssetStatusRequests.Find(InAssetPath))
	{
		// Check if the current request hasn't expired.
		if (CurrentTime < PendingRequest->ExpirationTime)
		{
			// A "force refresh" request will override a non-"force refresh" one.
			if (PendingRequest->bForceRefresh >= bInForceRefresh)
			{
				return;
			}
		}
	}

	// Keep track of the request and the time it was made so we have a timeout.
	const FDateTime ExpirationTime = CurrentTime + FTimespan::FromSeconds(UAvaMediaSettings::Get().ClientPendingStatusRequestTimeout);
	ServerInfo->PendingPlaybackAssetStatusRequests.Add(InAssetPath, {ExpirationTime, bInForceRefresh} );

	FAvaPlaybackAssetStatusRequest* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackAssetStatusRequest>();
	Request->AssetPath = InAssetPath;
	Request->bForceRefresh = bInForceRefresh;
	SendRequest(Request, ServerInfo->Address);
}

void FAvaPlaybackClient::Tick()
{
	for (const TPair<FString, TSharedPtr<FServerInfo>>& Server : Servers)
	{
		if (!Server.Value->PendingPlaybackCommands.IsEmpty())
		{
			// Batched per server for now.
			FAvaPlaybackRequest* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackRequest>();
			Request->Commands = MoveTemp(Server.Value->PendingPlaybackCommands);
			Server.Value->PendingPlaybackCommands.Reset();
			SendRequest(Request, Server.Value->Address);
		}
	}
}

bool FAvaPlaybackClient::HandlePingTicker(float InDeltaTime)
{
	const FDateTime CurrentTime = FDateTime::UtcNow();
	PublishPlaybackPing(CurrentTime, true);
	RemoveDeadServers(CurrentTime);
	return true;
}

void FAvaPlaybackClient::HandlePlaybackPongMessage(const FAvaPlaybackPong& InMessage,
														const TSharedRef<IMessageContext, ESPMode::ThreadSafe>&	InContext)
{
	// Remark: the server will send a user data update before sending the pong message.
	// The server info will have been created already by this point. But it won't have all the information
	// complete yet.
	
	FServerInfo& ServerInfo = GetOrCreateServerInfo(InMessage.ServerName, InContext->GetSender());
	ServerInfo.ResetPingTimeout();

	// If the server info is new, the process id will not be set yet.
	const bool bIsNewServer = ServerInfo.ProcessId == 0 ? true : false;
	
	if (!bIsNewServer && ServerInfo.ProcessId != InMessage.ProcessId)
	{
		UE_LOG(LogAvaPlaybackClient, Warning, TEXT("Received server \"%s\" process Id has changed from %d to %d."),
			*InMessage.ServerName, ServerInfo.ProcessId, InMessage.ProcessId);
	}
	
	ServerInfo.ProcessId = InMessage.ProcessId;
	ServerInfo.ProjectContentPath = InMessage.ProjectContentPath;
	
	// The server may have requested the client info, and it has to be sent unless
	// this is a new server, in which case it has already been sent by OnServerAdded().
	if (InMessage.bRequestClientInfo && !bIsNewServer)
	{
		SendClientInfo(InContext->GetSender());
	}

	// We want to propagate the connection event after all the information has been set
	// in the server info.
	if (bIsNewServer)
	{
		using namespace UE::AvaPlaybackClient::Delegates;
		GetOnConnectionEvent().Broadcast(*this, {InMessage.ServerName, EConnectionEvent::ServerConnected});
	}
}

void FAvaPlaybackClient::HandlePlaybackLogMessage(const FAvaPlaybackLog& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	if (GLog && InMessage.Verbosity != 0)
	{
		// Basic message formatting: adding the server name to identify the origin of the message.
		const FString FormattedText = InMessage.ServerName + TEXT(" >> ") + InMessage.Text;

		// Relay to local GLog.
		GLog->Serialize(*FormattedText, static_cast<ELogVerbosity::Type>(InMessage.Verbosity), InMessage.Category, InMessage.Time);
	}
}

void FAvaPlaybackClient::HandleUpdateServerUserData(const FAvaPlaybackUpdateServerUserData& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	FServerInfo& ServerInfo = GetOrCreateServerInfo(InMessage.ServerName, InContext->GetSender());
	ServerInfo.ResetPingTimeout();
	ServerInfo.UserDataEntries = InMessage.UserDataEntries;

	// Logging when user data is updated (for debugging).
	UE_LOG(LogAvaPlaybackClient, Verbose, TEXT("Received new user data for server \"%s\"."), *InMessage.ServerName);
	for (const TPair<FString, FString>& UserData : ServerInfo.UserDataEntries)
	{
		UE_LOG(LogAvaPlaybackClient, Verbose, TEXT("User data \"%s\":\"%s\"."), *UserData.Key, *UserData.Value);
	}
}

void FAvaPlaybackClient::HandleStatStatus(const FAvaPlaybackStatStatus& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	// If the client couldn't run the command, but the serve did, we are going
	// to replicate the stats state on the client instead.
	if (InMessage.bClientStateReliable == false && InMessage.bCommandSucceeded)
	{
		UE_LOG(LogAvaPlaybackClient, Verbose,
			TEXT("Received reliable enabled runtime stats from server \"%s\"."), *InMessage.ServerName);
		IAvaModule::Get().OverwriteEnabledRuntimeStats(InMessage.EnabledRuntimeStats);
	}
}

void FAvaPlaybackClient::HandleDeviceProviderDataListMessage(const FAvaBroadcastDeviceProviderDataList& InMessage,
																  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UE_LOG(LogAvaPlaybackClient, Verbose, TEXT("Received Device Provider Data for server \"%s\"."),
		   *InMessage.ServerName);
	IAvaBroadcastDeviceProviderProxyManager& Manager = ParentModule->GetDeviceProviderProxyManager();
	Manager.Install(InMessage.ServerName, InMessage);

	{
		FServerInfo& ServerInfo = GetOrCreateServerInfo(InMessage.ServerName, InContext->GetSender());
		ServerInfo.bDeviceProviderInstalled = true;
	}

	// Check if we have installed all the connected server's device providers.
	bool bAllDeviceProvidersInstalled = true;
	{
		ForAllServers([&bAllDeviceProvidersInstalled](const FServerInfo& InServerInfo)
		{
			if (InServerInfo.bDeviceProviderInstalled == false)
			{
				UE_LOG(LogAvaPlaybackClient, Verbose,
					   TEXT("Device Provider Data for server \"%s\" hasn't been received yet."), *InServerInfo.ServerName);
				bAllDeviceProvidersInstalled = false;
			}
		});
	}

	if (bAllDeviceProvidersInstalled)
	{
		UE_LOG(LogAvaPlaybackClient, Verbose,
			   TEXT("All Device Providers Proxies are installed. Requesting broadcast status update..."));
		// Remark: We are waiting for all the device providers from all online servers to be
		// installed before requesting a broadcast status.
		// This is because handling the broadcast status update will load the broadcast
		// configuration. When the broadcast object is loaded, there is the potential for
		// legacy code to convert the config and it needs the device provider proxies to
		// be installed to resolve the device names and corresponding servers.
		ForAllServers([this](const FServerInfo& InServerInfo)
		{
			UE_LOG(LogAvaPlaybackClient, Verbose, TEXT("Requesting full broadcast status update for server \"%s\"."),
				   *InServerInfo.ServerName);
			// Also request broadcast channels status to update state of local channels if required.
			FAvaBroadcastStatusRequest* StatusRequest = FMessageEndpoint::MakeMessage<
				FAvaBroadcastStatusRequest>();
			StatusRequest->bIncludeMediaOutputData = true;
			// We will want to ensure our local config is the same as the servers.

			SendRequest(StatusRequest, InServerInfo.Address);
		});
	}
}

void FAvaPlaybackClient::HandleBroadcastStatusMessage(const FAvaBroadcastStatus& InMessage,
														   const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	using namespace UE::AvaPlaybackClient::Private;
	UE_LOG(LogAvaPlaybackClient, Verbose,
		   TEXT("Received broadcast update from server \"%s\" for channel \"%s\": Status: \"%s\"."),
		   *InMessage.ServerName, *InMessage.ChannelName,
		   *EnumToString(InMessage.ChannelState));

	// Log details for the output status.
	for (const TPair<FGuid, FAvaBroadcastOutputStatus>& OutputStatus : InMessage.MediaOutputStatuses)
	{
		UE_LOG(LogAvaPlaybackClient, Verbose,
			   TEXT("Playback Server: \"%s\" Channel: \"%s\" OutputId \"%s\" Status: \"%s\" Severity: \"%s\"."),
			   *InMessage.ServerName, *InMessage.ChannelName, *OutputStatus.Key.ToString(),
			   *EnumToString(OutputStatus.Value.MediaOutputState),
			   *EnumToString(OutputStatus.Value.MediaIssueSeverity));
		
		for (const FString& Message : OutputStatus.Value.MediaIssueMessages)
		{
			UE_LOG(LogAvaPlaybackClient, Verbose, TEXT("Output Issue Message: \"%s\"."), *Message);
		}
	}

	FServerInfo& ServerInfo = GetOrCreateServerInfo(InMessage.ServerName, InContext->GetSender());
	ServerInfo.ResetPingTimeout();

	const FString CurrentProfileName = UAvaBroadcast::Get().GetCurrentProfileName().ToString();

	{
		// Keep a back store of this locally, mainly so we can respond to UI requests
		// for the status of the media output objects.
		FBroadcastChannelInfo& ChannelInfo = ServerInfo.GetOrCreateBroadcastChannelInfo(InMessage.ChannelName);
		ChannelInfo.ChannelState = InMessage.ChannelState;
		ChannelInfo.ChannelIssueSeverity = InMessage.ChannelIssueSeverity;		
		ChannelInfo.MediaOutputStatuses = InMessage.MediaOutputStatuses;
	}

	{
		const FName ChannelName = *InMessage.ChannelName;
		FAvaBroadcastOutputChannel& Channel = UAvaBroadcast::Get().GetCurrentProfile().GetChannelMutable(ChannelName);
		if (Channel.IsValidChannel())
		{
			// Purpose of this section is to ensure the server and the client have
			// the same configuration. We want to perform this in the less intrusive
			// way possible, i.e. if the server is running and the client connects to it
			// we would like the server to be left completely undisturbed if it's configuration
			// is already synced.

			// Verify if the server has the corresponding outputs already.
			bool bServerIsMissingOutputs = false;
			int32 NumOutputsForThisServer = 0;
			TArray<UMediaOutput*> RemoteOutputs = Channel.GetRemoteMediaOutputs();
			for (const UMediaOutput* RemoteOutput : RemoteOutputs)
			{
				const FAvaBroadcastMediaOutputInfo& OutputInfo = Channel.GetMediaOutputInfo(RemoteOutput);
				if (OutputInfo.IsValid() && OutputInfo.ServerName == InMessage.ServerName)
				{
					++NumOutputsForThisServer;
					if (!InMessage.MediaOutputStatuses.Contains(OutputInfo.Guid))
					{
						bServerIsMissingOutputs = true;
					}
					else if (InMessage.bIncludeMediaOutputData)
					{
						// The server already has this output, but we need to check
						// if it is the same. TODO.
					}
				}
			}

			if ((InMessage.bIncludeMediaOutputData && NumOutputsForThisServer > 0 && InMessage.MediaOutputs.Num() == 0)
				|| bServerIsMissingOutputs)
			{
				UE_LOG(LogAvaPlaybackClient, Log,
					   TEXT("Playback Server: \"%s\" Channel: \"%s\" is missing outputs. Requesting configuration update."),
					   *InMessage.ServerName, *InMessage.ChannelName);

				RequestBroadcast(CurrentProfileName, ChannelName, RemoteOutputs, EAvaBroadcastAction::UpdateConfig);
			}

			// Note: this will broadcast to delegates which may then request states of media outputs.
			// So the back store needs to be updated before calling this.
			Channel.RefreshState();

			// Explicitly call the broadcast of the channel state change to propagate the status of the output,
			// which is not fully reflected in the channel state.
			FAvaBroadcastOutputChannel::GetOnChannelChanged().Broadcast(Channel, EAvaBroadcastChannelChange::State);
		}
		else
		{
			UE_LOG(LogAvaPlaybackClient, Error,
				   TEXT("Received broadcast update from server \"%s\" for locally invalid channel \"%s\"."),
				   *InMessage.ServerName, *InMessage.ChannelName);

			// Request this channel be deleted.
			RequestBroadcast(CurrentProfileName, ChannelName, {}, EAvaBroadcastAction::DeleteChannel);
		}
	}

	// Check for missing channels (is this logic valid?)
	if (InMessage.ChannelIndex == InMessage.NumChannels - 1)
	{
		// We have received the last channel this server has, so we can check in the server state
		// if it has all the channels it is supposed to have.
		const TArray<FAvaBroadcastOutputChannel*>& Channels = UAvaBroadcast::Get().GetCurrentProfile().GetChannels();
		for (const FAvaBroadcastOutputChannel* Channel : Channels)
		{
			bool bHasOutputsForThisServer = false;
			TArray<UMediaOutput*> RemoteOutputs = Channel->GetRemoteMediaOutputs();
			for (const UMediaOutput* RemoteOutput : RemoteOutputs)
			{
				const FAvaBroadcastMediaOutputInfo& OutputInfo = Channel->GetMediaOutputInfo(RemoteOutput);
				if (OutputInfo.IsValid() && OutputInfo.ServerName == InMessage.ServerName)
				{
					bHasOutputsForThisServer = true;
					break;
				}
			}
			if (bHasOutputsForThisServer)
			{
				if (ServerInfo.GetBroadcastChannelInfo(Channel->GetChannelName().ToString()) == nullptr)
				{
					// The server is missing this channel, we need to update it's config.
					RequestBroadcast(CurrentProfileName, Channel->GetChannelName(), RemoteOutputs, EAvaBroadcastAction::UpdateConfig);
				}
			}
		}
	}
}

void FAvaPlaybackClient::HandlePlaybackAssetStatusMessage(
	const FAvaPlaybackAssetStatus& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	using namespace UE::AvaPlaybackClient::Private;
	using namespace UE::AvaPlaybackClient::Delegates;
	if (!InMessage.AssetPath.IsValid())
	{
		UE_LOG(LogAvaPlaybackClient, Error,
			   TEXT("Received Playback Asset Status \"%s\" from server \"%s\" with invalid asset path, can't update status."),
			   *EnumToString(InMessage.Status), *InMessage.ServerName);
		return;
	}
	
	FServerInfo& ServerInfo = GetOrCreateServerInfo(InMessage.ServerName, InContext->GetSender());
	ServerInfo.ResetPingTimeout();	
	ServerInfo.SetPlaybackAssetStatus(InMessage.AssetPath, InMessage.Status);
		
	// Clear the pending request (if any).
	ServerInfo.PendingPlaybackAssetStatusRequests.Remove(InMessage.AssetPath);

	GetOnPlaybackAssetStatusChanged().Broadcast(*this,{InMessage.AssetPath, InMessage.ServerName, InMessage.Status});

	// TODO - Further refactoring needed to untangle playback state and asset state. 
	// Because some of the states of the playback status reflect the state of the asset on disk,
	// we need to make sure the playback state properly reflects the asset state for those cases.
	const bool bAssetIsMissing = InMessage.Status == EAvaPlaybackAssetStatus::Missing;
	const bool bAssetIsAvailable = InMessage.Status == EAvaPlaybackAssetStatus::Available
									|| InMessage.Status == EAvaPlaybackAssetStatus::MissingDependencies;

	for (const TPair<FString, TUniquePtr<FPlaybackChannelInfo>>& ChannelInfo : ServerInfo.PlaybackChannelInfosByName)
	{
		check(ChannelInfo.Value.IsValid());
		if (FPlaybackAssetInfo* AssetInfo = ChannelInfo.Value->GetAssetInfo(InMessage.AssetPath))
		{
			auto ConditionalChangeStatus = [AssetInfo, &InMessage, &ChannelInfo, bAssetIsMissing, bAssetIsAvailable, this](const FGuid& InInstanceId, EAvaPlaybackStatus InPlaybackStatus)
			{
				// A missing asset leads to a missing playback.
				if (InPlaybackStatus == EAvaPlaybackStatus::Available && bAssetIsMissing)
				{
					AssetInfo->SetInstanceStatus(InInstanceId, EAvaPlaybackStatus::Missing);
					GetOnPlaybackStatusChanged().Broadcast(*this, {InInstanceId, InMessage.AssetPath, ChannelInfo.Key, InMessage.ServerName, InPlaybackStatus,EAvaPlaybackStatus::Missing});
				}
				// If the playback was missing, and the asset becomes available, update the playback to available too.
				else if (InPlaybackStatus == EAvaPlaybackStatus::Missing && bAssetIsAvailable)
				{
					AssetInfo->SetInstanceStatus(InInstanceId, EAvaPlaybackStatus::Available);
					GetOnPlaybackStatusChanged().Broadcast(*this, {InInstanceId, InMessage.AssetPath, ChannelInfo.Key, InMessage.ServerName, InPlaybackStatus,EAvaPlaybackStatus::Available});
				}
			};

			for (TPair<FGuid, FPlaybackInstanceInfo>& InstanceInfo : AssetInfo->InstanceByIds)
			{
				ConditionalChangeStatus(InstanceInfo.Key, InstanceInfo.Value.Status);
			}
		}
	}
	
	UE_LOG(LogAvaPlaybackClient, Verbose,
		   TEXT("Received Playback Asset Status \"%s\" from server \"%s\" for \"%s\"."),
		   *EnumToString(InMessage.Status),
		   *InMessage.ServerName, *InMessage.AssetPath.ToString());
}

void FAvaPlaybackClient::HandlePlaybackStatusMessage(const FAvaPlaybackStatus& InMessage,
														  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	FServerInfo& ServerInfo = GetOrCreateServerInfo(InMessage.ServerName, InContext->GetSender());
	ServerInfo.ResetPingTimeout();
	
	if (InMessage.AssetPath.IsValid())
	{
		TOptional<EAvaPlaybackStatus> PrevPlaybackStatus =
			ServerInfo.GetInstanceStatus(InMessage.ChannelName, InMessage.InstanceId, InMessage.AssetPath);
		if (!PrevPlaybackStatus.IsSet())
		{
			PrevPlaybackStatus = EAvaPlaybackStatus::Unknown;
		}
		
		ServerInfo.SetInstanceStatus(InMessage.ChannelName, InMessage.InstanceId, InMessage.AssetPath, InMessage.Status);
		
		if (InMessage.bValidUserData)
		{
			ServerInfo.SetInstanceUserData(InMessage.ChannelName, InMessage.InstanceId, InMessage.AssetPath, InMessage.UserData);
		}

		// Clear the pending request (if any).
		PendingPlaybackStatusRequests.Remove(GetPlaybackStatusRequestKey(InMessage.InstanceId, InMessage.AssetPath, InMessage.ChannelName));

		UE::AvaPlaybackClient::Delegates::GetOnPlaybackStatusChanged().Broadcast(
			*this, {InMessage.InstanceId, InMessage.AssetPath, InMessage.ChannelName, InMessage.ServerName, PrevPlaybackStatus.GetValue(), InMessage.Status});
		
		UE_LOG(LogAvaPlaybackClient, Verbose,
			   TEXT("Received Playback Status \"%s\" from server.channel \"%s.%s\" for \"%s\"."),
			   *StaticEnum<EAvaPlaybackStatus>()->GetValueAsString(InMessage.Status),
			   *InMessage.ServerName, *InMessage.ChannelName, *InMessage.AssetPath.ToString());
	}
	else
	{
		UE_LOG(LogAvaPlaybackClient, Error,
			   TEXT(
				   "Received Playback Status \"%s\" from server.channel \"%s.%s\" with invalid asset path, can't update status."
			   ),
			   *StaticEnum<EAvaPlaybackStatus>()->GetValueAsString(InMessage.Status), *InMessage.ServerName,
			   *InMessage.ChannelName);
	}
}

void FAvaPlaybackClient::HandlePlaybackStatusesMessage(const FAvaPlaybackStatuses& InMessage,
															const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	FServerInfo& ServerInfo = GetOrCreateServerInfo(InMessage.ServerName, InContext->GetSender());
	ServerInfo.ResetPingTimeout();

	const int32 NumStatuses = FMath::Max(InMessage.AssetPaths.Num(), InMessage.InstanceIds.Num());
	for (int32 StatusIndex = 0; StatusIndex < NumStatuses; StatusIndex++)
	{
		const FSoftObjectPath AssetPath = InMessage.AssetPaths.IsValidIndex(StatusIndex) ? InMessage.AssetPaths[StatusIndex] : FSoftObjectPath();
		const FGuid InstanceId = InMessage.InstanceIds.IsValidIndex(StatusIndex) ? InMessage.InstanceIds[StatusIndex] : FGuid();
		
		if (AssetPath.IsValid())
		{
			TOptional<EAvaPlaybackStatus> PrevPlaybackStatus = ServerInfo.GetInstanceStatus(InMessage.ChannelName, InstanceId, AssetPath);
			if (!PrevPlaybackStatus.IsSet())
			{
				PrevPlaybackStatus = EAvaPlaybackStatus::Unknown;
			}

			ServerInfo.SetInstanceStatus(InMessage.ChannelName, InstanceId, AssetPath, InMessage.Status);

			// Clear the pending request (if any).
			PendingPlaybackStatusRequests.Remove(GetPlaybackStatusRequestKey(InstanceId, AssetPath, InMessage.ChannelName));

			UE::AvaPlaybackClient::Delegates::GetOnPlaybackStatusChanged().Broadcast(
				*this, {InstanceId, AssetPath, InMessage.ChannelName, InMessage.ServerName, PrevPlaybackStatus.GetValue(), InMessage.Status});
			
			UE_LOG(LogAvaPlaybackClient, Verbose,
				   TEXT("Received Playback Status \"%s\" from server.channel \"%s.%s\" for \"%s\"."),
				   *StaticEnum<EAvaPlaybackStatus>()->GetValueAsString(InMessage.Status),
				   *InMessage.ServerName, *InMessage.ChannelName, *AssetPath.ToString());
		}
		else
		{
			UE_LOG(LogAvaPlaybackClient, Error,
				   TEXT(
					   "Received Playback Status \"%s\" from server.channel \"%s.%s\" with invalid asset path, can't update status."
				   ),
				   *StaticEnum<EAvaPlaybackStatus>()->GetValueAsString(InMessage.Status), *InMessage.ServerName,
				   *InMessage.ChannelName);
		}
	}
}

void FAvaPlaybackClient::HandlePlaybackSequenceEventMessage(const FAvaPlaybackSequenceEvent& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	using namespace UE::AvaPlaybackClient::Delegates;
	const FPlaybackSequenceEventArgs Args =
	{
		InMessage.InstanceId,
		InMessage.AssetPath,
		InMessage.ChannelName,
		InMessage.ServerName,
		InMessage.SequenceName,
		InMessage.EventType
	};
	GetOnPlaybackSequenceEvent().Broadcast(*this, Args);
}

void FAvaPlaybackClient::HandlePlaybackTransitionEventMessage(const FAvaPlaybackTransitionEvent& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	using namespace UE::AvaPlaybackClient::Delegates;
	const FPlaybackTransitionEventArgs Args =
	{
		InMessage.TransitionId,
		InMessage.InstanceId,
		InMessage.ChannelName,
		InMessage.ServerName,
		InMessage.GetEventFlags()
	};
	GetOnPlaybackTransitionEvent().Broadcast(*this, Args);
}

void FAvaPlaybackClient::RegisterCommands()
{
	if (ConsoleCommands.Num() != 0)
	{
		return;
	}

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MotionDesignPlaybackClient.PingServers"),
		TEXT("Requests servers to give their information."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaPlaybackClient::PingServersCommand),
		ECVF_Default
	));
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MotionDesignPlaybackClient.RefreshAssetStatus"),
		TEXT("Request a refresh of the asset status."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaPlaybackClient::RequestPlaybackAssetStatusCommand),
		ECVF_Default
	));
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MotionDesignPlaybackClient.PlaybackRequest"),
		TEXT("Request Connected Servers to execute a playback request."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaPlaybackClient::RequestPlaybackCommand),
		ECVF_Default
	));
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MotionDesignPlaybackClient.BroadcastRequest"),
		TEXT("Request Connected Servers to execute a broadcast request."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaPlaybackClient::RequestBroadcastCommand),
		ECVF_Default
	));
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MotionDesignPlaybackClient.SetUserData"),
		TEXT("Set Replicated User Data Entry (Key, Value)."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaPlaybackClient::SetUserDataCommand),
		ECVF_Default
	));
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MotionDesignPlaybackClient.Status"),
		TEXT("Display current status of all server info."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaPlaybackClient::ShowStatusCommand),
		ECVF_Default
	));
}

namespace UE::AvaPlaybackClient::Private
{
	inline FString GetCommaSeparatedList(const UEnum* InEnum)
	{
		FString List;
		const int32 NumEnums = InEnum->ContainsExistingMax() ? InEnum->NumEnums() - 1 : InEnum->NumEnums();
		for (int32 EnumIndex = 0; EnumIndex < NumEnums; ++EnumIndex)
		{
			if (!List.IsEmpty())
			{
				List += TEXT(", ");
			}
			List += InEnum->GetNameStringByIndex(EnumIndex);
		}
		return List;
	}
}

void FAvaPlaybackClient::RequestPlaybackAssetStatusCommand(const TArray<FString>& InArgs)
{
	using namespace UE::AvaPlaybackClient::Private;
	if (Servers.Num() == 0)
	{
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("No servers available. Do AvaMediaClient.PingServers."));
		return;
	}

	if (InArgs.Num() == 0)
	{
		UE_LOG(LogAvaPlaybackClient, Log,
			   TEXT("Arguments: [AssetPath] [ChannelOrServer]. Ex: \"/Game/AvaPlayback.AvaPlayback Channel0\""));
	}

	FString ChannelOrServerName;
	FSoftObjectPath AssetPath;

	if (InArgs.Num() > 0)
	{
		AssetPath = FSoftObjectPath(FTopLevelAssetPath(InArgs[0]));
	}
	
	if (InArgs.Num() > 1)
	{
		ChannelOrServerName = InArgs[1];
	}
	else
	{
		ChannelOrServerName = UAvaBroadcast::Get().GetChannelName(0).ToString();
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("No Channel specified, using \"%s\"."), *ChannelOrServerName);
	}

	if (AssetPath.IsValid())
	{
		RequestPlaybackAssetStatus(AssetPath, ChannelOrServerName, true);
	}
	else
	{
		// Request a refresh of all cached assets for the specified server/channel.
		ForAllServers([&ChannelOrServerName, this](const FServerInfo& InServerInfo)
		{			
			if (InServerInfo.ServerName ==  ChannelOrServerName || InServerInfo.BroadcastChannelInfosByName.Contains(ChannelOrServerName))
			{
				for (const TPair<FSoftObjectPath, EAvaPlaybackAssetStatus>& AssetStatus : InServerInfo.PlaybackAssetStatuses)
				{
					RequestPlaybackAssetStatus(AssetStatus.Key, ChannelOrServerName, true);
				}
			}
		});
	}
}

void FAvaPlaybackClient::RequestPlaybackCommand(const TArray<FString>& InArgs)
{
	using namespace UE::AvaPlaybackClient::Private;
	if (Servers.Num() == 0)
	{
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("No servers available. Do AvaMediaClient.PingServers."));
		return;
	}

	if (InArgs.Num() == 0)
	{
		UE_LOG(LogAvaPlaybackClient, Log,
			   TEXT("Arguments: Action [Package] [AssetName]. Ex: \"Start /Game/AvaPlayback AvaPlayback\""));
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("Action: %s"), *GetCommaSeparatedList(StaticEnum<EAvaPlaybackAction>()));
		return;
	}

	const int64 PlaybackActionValue = StaticEnum<EAvaPlaybackAction>()->GetValueByNameString(InArgs[0]);
	if (PlaybackActionValue == INDEX_NONE)
	{
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("Action: \"%s\" is not valid. Possible actions: %s"),
			   *InArgs[0], *GetCommaSeparatedList(StaticEnum<EAvaPlaybackAction>()));
		return;
	}

	const EAvaPlaybackAction PlaybackAction = static_cast<EAvaPlaybackAction>(PlaybackActionValue);

	if (InArgs.Num() >= 3)
	{
		//Concatenate all Args (starting from the 3rd) into one String with spaces in between each arg
		FString ConcatenatedCommands;
		for (int32 Index = 3; Index < InArgs.Num(); ++Index)
		{
			ConcatenatedCommands += InArgs[Index] + TEXT(" ");
		}

		FString ChannelName;
		FParse::Value(*ConcatenatedCommands, TEXT("Channel="), ChannelName);

		FString InstanceId;
		FParse::Value(*ConcatenatedCommands, TEXT("InstandId="), InstanceId);
		
		const FSoftObjectPath AssetPath(InArgs[1] + TEXT(".") + InArgs[2]);
		RequestPlayback(FGuid(InstanceId), AssetPath, ChannelName, PlaybackAction);
	}
	else
	{
		RequestPlayback(FGuid(), FSoftObjectPath(), TEXT(""), PlaybackAction);
	}
}

void FAvaPlaybackClient::RequestBroadcastCommand(const TArray<FString>& InArgs)
{
	using namespace UE::AvaPlaybackClient::Private;

	if (Servers.Num() == 0)
	{
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("No servers available."));
		return;
	}

	if (InArgs.Num() == 0 || InArgs.Num() > 1)
	{
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("Arguments: Action.  Possible actions: %s"),
			   *GetCommaSeparatedList(StaticEnum<EAvaBroadcastAction>()));
		return;
	}

	const int64 BroadcastActionValue = StaticEnum<EAvaBroadcastAction>()->GetValueByNameString(InArgs[0]);
	if (BroadcastActionValue == INDEX_NONE)
	{
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("Action: \"%s\" is not valid. Possible actions: %s"),
			   *InArgs[0], *GetCommaSeparatedList(StaticEnum<EAvaBroadcastAction>()));
		return;
	}
	
	RequestBroadcast(TEXT(""), FName(), TArray<UMediaOutput*>(), static_cast<EAvaBroadcastAction>(BroadcastActionValue));
}

void FAvaPlaybackClient::SetUserDataCommand(const TArray<FString>& InArgs)
{
	if (InArgs.Num() >= 2)
	{
		UE_LOG(LogAvaPlaybackClient, Log, TEXT("Setting User Data Key \"%s\" to Value: \"%s\"."), *InArgs[0], *InArgs[1]);
		SetUserData(InArgs[0], InArgs[1]);
	}
	else if (InArgs.Num() == 1)
	{
		// One argument means to remove that user data entry.
		if (HasUserData(InArgs[0]))
		{
			UE_LOG(LogAvaPlaybackClient, Log, TEXT("Removing User Data Key \"%s\"."), *InArgs[0]);
			RemoveUserData(InArgs[0]);
		}
		else
		{
			UE_LOG(LogAvaPlaybackClient, Error, TEXT("User Data Key \"%s\" not found."), *InArgs[0]);
		}
	}
}

void FAvaPlaybackClient::ShowStatusCommand(const TArray<FString>& InArgs)
{
	using namespace UE::AvaPlaybackClient::Private;
	UE_LOG(LogAvaPlaybackClient, Display, TEXT("Playback Client: \"%s\""), *ComputerName);
	UE_LOG(LogAvaPlaybackClient, Display, TEXT("- Endpoint Bus Address: \"%s\""), MessageEndpoint.IsValid() ? *MessageEndpoint->GetAddress().ToString() : TEXT("Invalid"));
	UE_LOG(LogAvaPlaybackClient, Display, TEXT("- ProcessId: %d"), ProcessId);
	UE_LOG(LogAvaPlaybackClient, Display, TEXT("- Content Path: \"%s\""), *ProjectContentPath);

	for (const TPair<FString, FString>& UserData : UserDataEntries)
	{
		UE_LOG(LogAvaPlaybackClient, Display, TEXT("- User data \"%s\":\"%s\"."), *UserData.Key, *UserData.Value);
	}
	
	for (const TPair<FString, TSharedPtr<FServerInfo>>& Server : Servers)
	{
		check(Server.Value.IsValid());
		const FServerInfo& ServerInfo = *Server.Value;
		UE_LOG(LogAvaPlaybackClient, Display, TEXT("Connected Server: \"%s\""), *ServerInfo.ServerName);
		
		UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Endpoint Bus Address: \"%s\""), *ServerInfo.Address.ToString());
		UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - ProcessId: %d"), ServerInfo.ProcessId);
		UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Content Path: \"%s\""), *ServerInfo.ProjectContentPath);
		
		for (const TPair<FString, FString>& UserData : ServerInfo.UserDataEntries)
		{
			UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - User data \"%s\":\"%s\"."), *UserData.Key, *UserData.Value);
		}
		for (const TPair<FString, TUniquePtr<FBroadcastChannelInfo>>& ChannelInfo : ServerInfo.BroadcastChannelInfosByName)
		{
			check(ChannelInfo.Value.IsValid());
			UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Channel(\"%s\") Channel State: \"%s\"."),
				*ChannelInfo.Key,
				*EnumToString(ChannelInfo.Value->ChannelState));
			UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Channel(\"%s\") Channel Issue Severity: \"%s\"."),
				*ChannelInfo.Key,
				*EnumToString(ChannelInfo.Value->ChannelIssueSeverity));
			for (const TPair<FGuid, FAvaBroadcastOutputStatus>& MediaOutputStatus : ChannelInfo.Value->MediaOutputStatuses)
			{
				UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Channel(\"%s\") Media Output \"%s\" State: \"%s\"."),
					*ChannelInfo.Key, *MediaOutputStatus.Key.ToString(),
					*EnumToString(MediaOutputStatus.Value.MediaOutputState));
				UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Channel(\"%s\") Media Output \"%s\" Issue Severity: \"%s\"."),
					*ChannelInfo.Key, *MediaOutputStatus.Key.ToString(),
					*EnumToString(MediaOutputStatus.Value.MediaIssueSeverity));
				for (const FString& MediaIssueMessage : MediaOutputStatus.Value.MediaIssueMessages)
				{
					UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Channel(\"%s\") Media Output \"%s\" Message: \"%s\"."),
						*ChannelInfo.Key, *MediaOutputStatus.Key.ToString(), *MediaIssueMessage);
				}
			}
		}
		for (const TPair<FString, TUniquePtr<FPlaybackChannelInfo>>& ChannelInfoEntry : ServerInfo.PlaybackChannelInfosByName)
		{
			check(ChannelInfoEntry.Value.IsValid());
			const FPlaybackChannelInfo& ChannelInfo = *ChannelInfoEntry.Value;
			for (const TPair<FSoftObjectPath, TUniquePtr<FPlaybackAssetInfo>>& AssetInfo : ChannelInfo.AssetInfoByPaths)
			{
				// Dump per instance statuses.
				for (const TPair<FGuid, FPlaybackInstanceInfo>& InstanceInfo : AssetInfo.Value->InstanceByIds)
				{
					// Check if we have user data.
					FString PrettyPrintUserData = InstanceInfo.Value.UserData.IsSet() ?
						FString::Printf(TEXT(" - UserData: %s"), *InstanceInfo.Value.UserData.GetValue()) : FString();
					
					UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Channel(\"%s\") Playback \"%s\"(instance \"%s\") Status:\"%s\"%s."),
						*ChannelInfoEntry.Key,
						*AssetInfo.Key.ToString(),
						*InstanceInfo.Key.ToString(),
						*EnumToString(InstanceInfo.Value.Status),
						*PrettyPrintUserData);
				}
			}
		}
		for (const TPair<FSoftObjectPath, EAvaPlaybackAssetStatus>& AssetStatus : ServerInfo.PlaybackAssetStatuses)
		{
			UE_LOG(LogAvaPlaybackClient, Display, TEXT("   - Asset (\"%s\") Status \"%s\"."),
				*AssetStatus.Key.ToString(),
				*EnumToString(AssetStatus.Value));
		}
	}
}

void FAvaPlaybackClient::OnBroadcastChanged(EAvaBroadcastChange InChange)
{
	if (EnumHasAnyFlags(InChange, EAvaBroadcastChange::CurrentProfile))
	{
		// Propagate the new output configuration to the connected servers of this channel.
		// We can do this because the profile can only be switched if channels are idle.
		FAvaBroadcastProfile& CurrentProfile = UAvaBroadcast::Get().GetCurrentProfile();
		const TArray<FAvaBroadcastOutputChannel*>& Channels = CurrentProfile.GetChannels();
		for (const FAvaBroadcastOutputChannel* Channel : Channels)
		{
			TArray<UMediaOutput*> RemoteOutputs = Channel->GetRemoteMediaOutputs();
			if (!RemoteOutputs.IsEmpty())
			{
				const FString ProfileName = CurrentProfile.GetName().ToString();
				RequestBroadcast(ProfileName, Channel->GetChannelName(), RemoteOutputs, EAvaBroadcastAction::UpdateConfig);
			}
		}
	}
}

void FAvaPlaybackClient::OnChannelChanged(const FAvaBroadcastOutputChannel& InChannel, EAvaBroadcastChannelChange InChange)
{
	if (EnumHasAnyFlags(InChange, EAvaBroadcastChannelChange::Settings))
	{
		SendBroadcastChannelSettingsUpdate(GetServerAddressesForChannel(InChannel.GetChannelName()), InChannel);
	}
}

void FAvaPlaybackClient::OnAvaMediaSettingsChanged(UObject*, struct FPropertyChangedEvent&)
{
	ApplyAvaMediaSettings();
	SendBroadcastSettingsUpdate(AllServerAddresses);
	SendAvaInstanceSettingsUpdate(AllServerAddresses);
}

void FAvaPlaybackClient::OnPreSavePackage(UPackage* InPackage, FObjectPreSaveContext InObjectSaveContext)
{
	// Only execute if this is a user save
	if (InObjectSaveContext.IsProceduralSave())
	{
		return;
	}

	// Early return is no servers are connected.
	if (Servers.IsEmpty())
	{
		return;
	}

	// Propagate this to inform local servers to flush loaders (and unlock the files).
	// Remark: The server may already flush the playback asset's package loaders (if enabled) but this path will
	// cover any other assets (not playback) that might have been loaded by both the server and client. 
	SendPackageEvent(AllServerAddresses, InPackage->GetFName(), EAvaPlaybackPackageEvent::PreSave);
}

void FAvaPlaybackClient::OnPackageSaved(const FString& InPackageFileName, UPackage* InPackage, FObjectPostSaveContext InObjectSaveContext)
{
	// Only execute if this is a user save
	if (InObjectSaveContext.IsProceduralSave())
	{
		return;
	}

	// Early return is no servers are connected.
	if (Servers.IsEmpty())
	{
		return;
	}

	// Propagate this to inform local servers to reload the package.
	SendPackageEvent(AllServerAddresses, InPackage->GetFName(), EAvaPlaybackPackageEvent::PostSave);
	
	TArray<FAssetData> AssetsInPackage;
	if (const IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetsInPackage.Reserve(16);
		AssetRegistry->GetAssetsByPackageName(InPackage->GetFName(), AssetsInPackage);
		
		if (AssetsInPackage.IsEmpty())
		{
			UE_LOG(LogAvaPlaybackClient, Warning, TEXT("Asset Registry returns no asset data for package \"%s\"."), *InPackage->GetName());
		}
	}
	
	// If a local package (of a playback asset) was saved, we will refresh the status of the assets with the servers.
	ForAllServers([&AssetsInPackage, this](const FServerInfo& InServerInfo)
	{
		for (const FAssetData& Assets : AssetsInPackage)
		{
			if (InServerInfo.PlaybackAssetStatuses.Contains(Assets.ToSoftObjectPath()))
			{
				// Note: we force a refresh (i.e. disregard the server's cached value) because the asset has changed on client side.
				RequestPlaybackAssetStatusForServer(Assets.ToSoftObjectPath(), InServerInfo.ServerName, true);
			}
		}
	});
}

void FAvaPlaybackClient::OnAssetRemoved(const FAssetData& InAssetData)
{
	// Early return is no servers are connected.
	if (Servers.IsEmpty())
	{
		return;
	}

	// Propagate this to inform local servers to purge the package.
	SendPackageEvent(AllServerAddresses, InAssetData.PackageName, EAvaPlaybackPackageEvent::AssetDeleted);
}

void FAvaPlaybackClient::ApplyAvaMediaSettings()
{
	const UAvaMediaSettings& Settings = UAvaMediaSettings::Get();
#if !NO_LOGGING
	if (Settings.bVerbosePlaybackClientLogging)
	{
		LogAvaPlaybackClient.SetVerbosity(ELogVerbosity::Verbose);
	}
	else
	{
		LogAvaPlaybackClient.SetVerbosity(ELogVerbosity::Log);
	}
#endif
}

void FAvaPlaybackClient::PublishPlaybackPing(const FDateTime& InCurrentTime, bool bInAutoPing)
{
	if (MessageEndpoint.IsValid())
	{
		const UAvaMediaSettings& AvaMediaSettings = UAvaMediaSettings::Get();
		// Add a timeout to all known servers
		const FDateTime Timeout = InCurrentTime + FTimespan::FromSeconds(AvaMediaSettings.PingTimeoutInterval);
		ForAllServers([&Timeout](FServerInfo& InServerInfo)
		{
			InServerInfo.AddTimeout(Timeout);
		});

		FAvaPlaybackPing* PlaybackPingMessage = FMessageEndpoint::MakeMessage<FAvaPlaybackPing>();
		PlaybackPingMessage->bAutoPing = bInAutoPing;
		PlaybackPingMessage->PingIntervalSeconds = AvaMediaSettings.PingInterval;
		PublishRequest(PlaybackPingMessage);
	}
}

void FAvaPlaybackClient::SendUserDataUpdate(const TArray<FMessageAddress>& InRecipients)
{
	FAvaPlaybackUpdateClientUserData* UserDataUpdate = FMessageEndpoint::MakeMessage<FAvaPlaybackUpdateClientUserData>();
	UserDataUpdate->UserDataEntries = UserDataEntries;
	SendRequest(UserDataUpdate, InRecipients, EMessageFlags::Reliable);
}

void FAvaPlaybackClient::SendBroadcastSettingsUpdate(const TArray<FMessageAddress>& InRecipients)
{
	const IAvaBroadcastSettings& BroadcastSettings = IAvaMediaModule::Get().GetBroadcastSettings();
	FAvaBroadcastSettingsUpdate* BroadcastSettingsUpdate = FMessageEndpoint::MakeMessage<FAvaBroadcastSettingsUpdate>();
	BroadcastSettingsUpdate->BroadcastSettings.ChannelClearColor = BroadcastSettings.GetChannelClearColor();
	BroadcastSettingsUpdate->BroadcastSettings.ChannelDefaultPixelFormat = BroadcastSettings.GetDefaultPixelFormat();
	BroadcastSettingsUpdate->BroadcastSettings.ChannelDefaultResolution = BroadcastSettings.GetDefaultResolution();
	BroadcastSettingsUpdate->BroadcastSettings.bDrawPlaceholderWidget = BroadcastSettings.IsDrawPlaceholderWidget();
	BroadcastSettingsUpdate->BroadcastSettings.PlaceholderWidgetClass = BroadcastSettings.GetPlaceholderWidgetClass();
	SendRequest(BroadcastSettingsUpdate, InRecipients, EMessageFlags::Reliable);
}

void FAvaPlaybackClient::SendAvaInstanceSettingsUpdate(const TArray<FMessageAddress>& InRecipients)
{
	FAvaPlaybackInstanceSettingsUpdate* InstanceSettingsUpdate = FMessageEndpoint::MakeMessage<FAvaPlaybackInstanceSettingsUpdate>();
	InstanceSettingsUpdate->InstanceSettings = UAvaMediaSettings::Get().AvaInstanceSettings;
	SendRequest(InstanceSettingsUpdate, InRecipients, EMessageFlags::Reliable);
}

void FAvaPlaybackClient::SendBroadcastChannelSettingsUpdate(const TArray<FMessageAddress>& InRecipients, const FAvaBroadcastOutputChannel& InChannel)
{
	FAvaBroadcastChannelSettingsUpdate* ChannelSettingsUpdate = FMessageEndpoint::MakeMessage<FAvaBroadcastChannelSettingsUpdate>();
	ChannelSettingsUpdate->Profile = InChannel.GetProfileName().ToString();
	ChannelSettingsUpdate->Channel = InChannel.GetChannelName().ToString();
	ChannelSettingsUpdate->QualitySettings = InChannel.GetViewportQualitySettings();
	SendRequest(ChannelSettingsUpdate, InRecipients, EMessageFlags::Reliable);
}

void FAvaPlaybackClient::SendPackageEvent(const TArray<FMessageAddress>& InRecipients, const FName& InPackageName, EAvaPlaybackPackageEvent InEvent)
{
	FAvaPlaybackPackageEvent* PackageEvent = FMessageEndpoint::MakeMessage<FAvaPlaybackPackageEvent>();
	PackageEvent->PackageName = InPackageName;
	PackageEvent->Event = InEvent;
	SendRequest(PackageEvent, InRecipients, EMessageFlags::Reliable);
}

void FAvaPlaybackClient::SendStatCommand(const FString& InCommand, bool bInBroadcastLocalState, const TArray<FMessageAddress>& InRecipients)
{
	FAvaPlaybackStatCommand* Request = FMessageEndpoint::MakeMessage<FAvaPlaybackStatCommand>();
	Request->Command = InCommand;
	Request->bClientStateReliable = bInBroadcastLocalState;
	Request->ClientEnabledRuntimeStats = IAvaModule::Get().GetEnabledRuntimeStats();
	SendRequest(Request, InRecipients);
}

void FAvaPlaybackClient::SendClientInfo(const FMessageAddress& InRecipient)
{
	FAvaPlaybackUpdateClientInfo* ClientInfo = FMessageEndpoint::MakeMessage<FAvaPlaybackUpdateClientInfo>();
	ClientInfo->ComputerName = ComputerName;
	ClientInfo->ProcessId = ProcessId;
	ClientInfo->ProjectContentPath = ProjectContentPath;
	SendRequest(ClientInfo, InRecipient);
	
	SendUserDataUpdate({InRecipient});
	SendBroadcastSettingsUpdate({InRecipient});
	SendAvaInstanceSettingsUpdate({InRecipient});
	SendStatCommand(FString(), true, {InRecipient});	// Send empty stat command, will just send current states.
}

void FAvaPlaybackClient::RemoveDeadServers(const FDateTime& InCurrentTime)
{
	bool bServerRemoved = false;

	for (TMap<FString, TSharedPtr<FServerInfo>>::TIterator ServerIter = Servers.CreateIterator(); ServerIter; ++
		 ServerIter)
	{
		check(ServerIter.Value().IsValid());
		if (ServerIter.Value()->HasTimedOut(InCurrentTime))
		{
			UE_LOG(LogAvaPlaybackClient, Log, TEXT("Server \"%s\" is not longer responding to pings. Removing."),
				   *ServerIter.Key());
			const TSharedPtr<FServerInfo> RemovedServer = ServerIter.Value();
			ServerIter.RemoveCurrent();
			OnServerRemoved(*RemovedServer);
			bServerRemoved = true;
		}
	}

	if (bServerRemoved)
	{
		UpdateServerAddresses();
	}
}

void FAvaPlaybackClient::UpdateServerAddresses()
{
	AllServerAddresses.Empty(Servers.Num());
	ForAllServers([this](const FServerInfo& InServerInfo)
	{
		AllServerAddresses.Add(InServerInfo.Address);
	});
}

TArray<FMessageAddress> FAvaPlaybackClient::GetServerAddressesForChannel(const FName& InChannelName) const
{
	if (!InChannelName.IsNone())
	{
		TArray<FString> ServerNames = GetServerNamesForChannel(InChannelName);
		TArray<FMessageAddress> ServerAddresses;
		ServerAddresses.Reserve(ServerNames.Num());
		for (const FString& ServerName : ServerNames)
		{
			if (const FServerInfo* ServerInfo = GetServerInfo(ServerName))
			{
				ServerAddresses.Add(ServerInfo->Address);
			}
		}
		return ServerAddresses;
	}

	// If no channel is specified, we will return all the servers that are connected.
	return AllServerAddresses;
}

TArray<FString> FAvaPlaybackClient::GetServerNamesForChannel(const FName& InChannelName) const
{
	TArray<FString> ServerNames;

	if (!InChannelName.IsNone())
	{
		const FAvaBroadcastOutputChannel& Channel = UAvaBroadcast::Get().GetCurrentProfile().GetChannel(InChannelName);
		const TArray<UMediaOutput*>& RemoteOutputs = Channel.GetRemoteMediaOutputs();
		ServerNames.Reserve(RemoteOutputs.Num());
		for (const UMediaOutput* RemoteOutput : RemoteOutputs)
		{
			const FAvaBroadcastMediaOutputInfo& OutputInfo = Channel.GetMediaOutputInfo(RemoteOutput);
			if (OutputInfo.IsValid())
			{
				ServerNames.Add(OutputInfo.ServerName);
			}
			else
			{
				UE_LOG(LogAvaPlaybackClient, Warning, TEXT("MediaOutputInfo invalid for channel \"%s\"."),
					   *InChannelName.ToString());

				// Try to find the server name from the device name. The server has to be online for this to work.
				const FString ServerName = GetServerNameForMediaOutputFallback(RemoteOutput);
				if (!ServerName.IsEmpty())
				{
					ServerNames.Add(ServerName);
				}
				else
				{
					UE_LOG(LogAvaPlaybackClient, Error,
						   TEXT("Unable to find server name for remote MediaOutput for channel \"%s\"."),
						   *InChannelName.ToString());
				}
			}
		}
	}
	else
	{
		// If no channel is specified, we will return all the servers that are connected.
		ServerNames = GetServerNames();
	}
	return ServerNames;
}

FString FAvaPlaybackClient::GetServerNameForMediaOutputFallback(const UMediaOutput* InMediaOutput) const
{
	const FString DeviceName = UE::AvaBroadcastOutputUtils::GetDeviceName(InMediaOutput);
	if (!DeviceName.IsEmpty())
	{
		// See if it begins with any of the remote server names that are connected.
		// See FAvaBroadcastDeviceProviderData::ApplyServerName. All MediaOutput from replicated Device Provider
		// Have the server name in the beginning of the device name.
		for (const TPair<FString, TSharedPtr<FServerInfo>>& Server : Servers)
		{
			if (DeviceName.StartsWith(Server.Key))
			{
				return Server.Key;
			}
		}
	}
	return FString();
}

FAvaPlaybackClient::FServerInfo& FAvaPlaybackClient::GetOrCreateServerInfo(
	const FString& InServerName, const FMessageAddress& InSenderAddress, bool* bOutCreated)
{
	if (FServerInfo* ServerInfo = GetServerInfo(InServerName))
	{
		if (ServerInfo->Address != InSenderAddress)
		{
			// This is suspicious though. It may also indicate a collision with multiple servers
			// with the same name on the same computer host.
			UE_LOG(LogAvaPlaybackClient, Warning, TEXT("Server \"%s\" Address changed."), *InServerName);
			ServerInfo->Address = InSenderAddress;
		}
		return *ServerInfo;
	}

	const TSharedPtr<FServerInfo> ServerInfo = MakeShared<FServerInfo>();
	ServerInfo->Address = InSenderAddress;
	ServerInfo->ServerName = InServerName;
	Servers.Add(InServerName, ServerInfo);
	
	UpdateServerAddresses();
	OnServerAdded(*ServerInfo);

	if (bOutCreated)
	{
		*bOutCreated = true;
	}
	return *ServerInfo;
}

void FAvaPlaybackClient::ForAllServers(TFunctionRef<void(FServerInfo& /*InServerInfo*/)> InFunction)
{
	for (const TPair<FString, TSharedPtr<FServerInfo>>& ServerInfo : Servers)
	{
		check(ServerInfo.Value.IsValid());
		InFunction(*ServerInfo.Value);
	}
}

void FAvaPlaybackClient::ForAllServers(TFunctionRef<void(const FServerInfo& /*InServerInfo*/)> InFunction) const
{
	for (const TPair<FString, TSharedPtr<FServerInfo>>& ServerInfo : Servers)
	{
		check(ServerInfo.Value.IsValid());
		InFunction(*ServerInfo.Value);
	}
}

void FAvaPlaybackClient::OnServerAdded(const FServerInfo& InServerInfo)
{
	UE_LOG(LogAvaPlaybackClient, Log, TEXT("Registering new playback server \"%s\"."), *InServerInfo.ServerName);
		
	FAvaPlaybackDeviceProviderDataRequest* DataRequest = FMessageEndpoint::MakeMessage<FAvaPlaybackDeviceProviderDataRequest>();
	SendRequest(DataRequest, InServerInfo.Address);
	
	SendClientInfo(InServerInfo.Address);
}

void FAvaPlaybackClient::OnServerRemoved(const FServerInfo& InRemovedServer)
{
	if (ParentModule)
	{
		IAvaBroadcastDeviceProviderProxyManager& Manager = ParentModule->GetDeviceProviderProxyManager();
		Manager.Uninstall(InRemovedServer.ServerName);
	}

	// Update the status (i.e. go offline) of channels for this server.
	TArray<FString> AffectedChannelNames;
	for (const TPair<FString, TUniquePtr<FBroadcastChannelInfo>>& ChannelInfo : InRemovedServer.BroadcastChannelInfosByName)
	{
		check(ChannelInfo.Value.IsValid());
		AffectedChannelNames.Add(ChannelInfo.Key);
	}

	FAvaBroadcastProfile& CurrentProfile = UAvaBroadcast::Get().GetCurrentProfile();
	
	// Try to reconcile channel state from remaining outputs (if any), i.e. channel is still partially online.
	for (const FString& AffectedChannelName : AffectedChannelNames)
	{
		FAvaBroadcastOutputChannel& Channel = CurrentProfile.GetChannelMutable(FName(AffectedChannelName));
		if (Channel.IsValidChannel())
		{
			// Note: this will broadcast to delegates which may then request states of media outputs.
			// So the back store needs to be updated before calling this.
			Channel.RefreshState();
		}
	}

	using namespace UE::AvaPlaybackClient::Delegates;
	GetOnConnectionEvent().Broadcast(*this, {InRemovedServer.ServerName, EConnectionEvent::ServerDisconnected});
}

const FAvaPlaybackClient::FBroadcastChannelInfo* FAvaPlaybackClient::GetChannelInfo(
	const FString& InServerName, const FString& InChannelName) const
{
	if (!InServerName.IsEmpty())
	{
		if (const FServerInfo* ServerInfo = GetServerInfo(InServerName))
		{
			return ServerInfo->GetBroadcastChannelInfo(InChannelName);
		}
	}
	else
	{
		// Legacy support: If the server name is not specified,
		// Find the first server with the given channel name.
		// Note: This doesn't work for "forked" channels.	
		for (const TPair<FString, TSharedPtr<FServerInfo>>& Server : Servers)
		{
			check(Server.Value.IsValid());
			if (const FBroadcastChannelInfo* ChannelInfo = Server.Value->GetBroadcastChannelInfo(InChannelName))
			{
				return ChannelInfo;
			}
		}
	}
	return nullptr;
}
