// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRundownMessages.h"
#include "Broadcast/AvaBroadcastProfile.h"
#include "MessageEndpoint.h"
#include "Rundown/AvaRundown.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"

class FAvaRundownManagedInstance;
class UMediaOutput;

/**
 * Implements a play list server that listens to commands on message bus.
 * The intention is to run a web socket transport bridge so the messages can
 * come from external applications.
 */
class FAvaRundownServer : public TSharedFromThis<FAvaRundownServer>
{
public:
	FAvaRundownServer();
	virtual ~FAvaRundownServer();

	void Init(const FString& InAssignedHostName);

	void SetupBroadcastDelegates(UAvaBroadcast* InBroadcast);
	void SetupEditorDelegates();
	void RemoveBroadcastDelegates(UAvaBroadcast* InBroadcast) const;
	void RemoveEditorDelegates() const;
	
	void OnPagesChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage, EAvaRundownPageChanges InChange) const;
	void OnPageListChanged(const FAvaRundownPageListChangeParams& InParams) const;
	void PageBlueprintChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage, const FString& InBlueprintPath) const;
	void PageStatusChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage) const;
	void PageChannelChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage, const FString& InChannelName) const;
	void PageAnimSettingsChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage) const;
	void OnBroadcastChannelListChanged(const FAvaBroadcastProfile& InProfile) const;
	void OnAssetAddedOrRemoved(const FAssetData& InAssetData) const;

	/** Returns the endpoint's message address. */
	const FMessageAddress& GetMessageAddress() const;
	
	// Message handlers
	void HandleRundownPing(const FAvaRundownPing& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetRundowns(const FAvaRundownGetRundowns& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleLoadRundown(const FAvaRundownLoadRundown& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetPages(const FAvaRundownGetPages& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleCreatePage(const FAvaRundownCreatePage& InMessage,const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleCreateTemplate(const FAvaRundownCreateTemplate& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleDeletePage(const FAvaRundownDeletePage& InMessage,const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleDeleteTemplate(const FAvaRundownDeleteTemplate& InMessage,const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleChangeTemplateBP(const FAvaRundownChangeTemplateBP& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetPageDetails(const FAvaRundownGetPageDetails& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleChangePageChannel(const FAvaRundownPageChangeChannel& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleUpdatePageFromRCP(const FAvaRundownUpdatePageFromRCP& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePageAction(const FAvaRundownPageAction& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePagePreviewAction(const FAvaRundownPagePreviewAction& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePageActions(const FAvaRundownPageActions& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandlePagePreviewActions(const FAvaRundownPagePreviewActions& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	void HandleGetChannel(const FAvaRundownGetChannel& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetChannels(const FAvaRundownGetChannels& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleChannelAction(const FAvaRundownChannelAction& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleAddChannelDevice(const FAvaRundownAddChannelDevice& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleEditChannelDevice(const FAvaRundownEditChannelDevice& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleRemoveChannelDevice(const FAvaRundownRemoveChannelDevice& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	void HandleGetChannelImage(const FAvaRundownGetChannelImage& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	
	void HandleGetDevices(const FAvaRundownGetDevices& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);
	
	void LogAndSendMessage(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InFormat, ...) const;
	void SendMessage(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InFormat, ...) const;
	void SendMessageImpl(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InMsg) const;

	void RegisterConsoleCommands();
	
	void ShowStatusCommand(const TArray<FString>& InArgs);

protected:
	struct FRequestInfo
	{
		int32 RequestId;
		FMessageAddress Sender;
	};
	
	struct FChannelImage;
	void FinishGetChannelImage(const FRequestInfo& InRequestInfo, const TSharedPtr<FChannelImage>& InChannelImage);

	void HandlePageActions(const FRequestInfo& InRequestInfo, const TArray<int32>& InPageIds,
		bool bInIsPreview, FName InPreviewChannelName, EAvaRundownPageActions InAction) const;
	
	/**
	 * Helper function to retrieve the appropriate rundown for editing commands.
	 */
	UAvaRundown* GetOrLoadRundownForEdit(const FMessageAddress& InSender, int32 InRequestId, const FString& InRundownPath);

	template<typename MessageType>
	void SendResponse(MessageType* InMessage, const FMessageAddress& InRecipient, EMessageFlags InFlags = EMessageFlags::None) const
	{
		if (MessageEndpoint)
		{
			MessageEndpoint->Send(InMessage, InRecipient);
		}
	}
	
	template<typename MessageType>
	void SendResponse(MessageType* InMessage, const TArray<FMessageAddress>& InRecipients, EMessageFlags InFlags = EMessageFlags::None) const
	{
		if (MessageEndpoint)
		{
			MessageEndpoint->Send(InMessage, MessageType::StaticStruct(), InFlags, nullptr,
				InRecipients, FTimespan::Zero(), FDateTime::MaxValue());
		}
	}

	void OnMessageBusNotification(const FMessageBusNotification& InNotification);

private:
	FString HostName;	
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;
	TArray<IConsoleObject*> ConsoleCommands;

	/** Keep information on connected clients. */
	struct FClientInfo
	{
		FMessageAddress Address;
		/** Api version for communication with this client. */
		int32 ApiVersion = -1;

		explicit FClientInfo(const FMessageAddress& InAddress) : Address(InAddress) {}
	};

	/** Keep track of remote clients context information. */
	TMap<FMessageAddress, TSharedPtr<FClientInfo>> Clients;

	/** Array of just the client addresses for sending responses. */
	TArray<FMessageAddress> ClientAddresses;
	
	FClientInfo* GetClientInfo(const FMessageAddress& InAddress) const
	{
		const TSharedPtr<FClientInfo>* ClientInfoPtr = Clients.Find(InAddress);
		return ClientInfoPtr ? ClientInfoPtr->Get() : nullptr;
	}
	
	FClientInfo& GetOrAddClientInfo(const FMessageAddress& InAddress)
	{
		if (const TSharedPtr<FClientInfo>* ExistingClientInfo = Clients.Find(InAddress))
		{
			return *ExistingClientInfo->Get();
		}

		const TSharedPtr<FClientInfo> NewClientInfo = MakeShared<FClientInfo>(InAddress);
		Clients.Add(InAddress, NewClientInfo);
		RefreshClientAddresses();
		return *NewClientInfo;
	}

	void RefreshClientAddresses();
	
	/** Pool of images that can be recycled. */
	TArray<TSharedPtr<FChannelImage>> AvailableChannelImages;
	
	/**
	 * Keeps a current rundown cached for commands operating on rundown.
	 * There is only one "current" rundown at a given time.
	 */
	struct FRundownCache
	{
		/** Currently loaded/cached rundown's path. */
		FSoftObjectPath CurrentRundownPath;
		
		/** Currently loaded/cached rundown object. */
		TStrongObjectPtr<UAvaRundown> CurrentRundown;

		FDelegateHandle OnPlaybackInstanceStatusChangedDelegateHandle;
		
		using FRundownEventFunction = TFunctionRef<void(UAvaRundown*)>;
		
		/**
		 * Returns requested rundown specified by InRundownPath. Will load it if necessary or returned the cached one if it is the same.
		 * If the new rundown fails to load, the return value is nullptr, and the previous rundown will remain loaded.
		 * @param InRundownPath	Requested rundown path. 
		 * @param InUnloadCurrentRundownFunction	Function called, with the previously cached rundown, when a new rundown is loaded
		 *											and previous rundown needs to be unloaded.
		 * @param InNewRundownLoadedFunction	Function called, with the new rundown, when a new rundown is loaded.
		 */
		UAvaRundown* GetOrLoadRundown(const FSoftObjectPath& InRundownPath, 
			const FRundownEventFunction InUnloadCurrentRundownFunction,
			const FRundownEventFunction InNewRundownLoadedFunction);

		void SetupRundownDelegates(FAvaRundownServer* InRundownServer, UAvaRundown* InRundown);
		void RemoveRundownDelegates(const FAvaRundownServer* InRundownServer, UAvaRundown* InRundown) const;
	};
	
	/** Cached data for page editing commands (GetPages and GetPageDetails). */
	struct FRundownEditCommandData : public FRundownCache
	{
		/** PageId of the current managed ava asset. */
		int32 ManagedPageId = FAvaRundownPage::InvalidPageId;
		TSharedPtr<FAvaRundownManagedInstance> ManagedInstance;

		~FRundownEditCommandData();
		
		/**
		 * Checks if previous RCP was registered.
		 * If so, save modified values to corresponding page.
 		 * This may result in the Rundown to be modified.
 		 * Will also unregister RCP from RC Module if requested.
		 */
		void SaveCurrentRemoteControlPresetToPage(bool bInUnregister);
	};
	FRundownEditCommandData RundownEditCommandData;

	/** Cached data for playback commands (LoadRundown and PageAction). */
	struct FRundownPlaybackCommandData: public FRundownCache
	{
		~FRundownPlaybackCommandData();

		void ClosePlaybackContext();
	};
	FRundownPlaybackCommandData RundownPlaybackCommandData;
};
