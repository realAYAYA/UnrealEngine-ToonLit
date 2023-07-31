// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialChatChannel.h"
#include "SocialToolkit.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineChatInterface.h"
#include "SocialReadOnlyChatChannel.h"
#include "SocialPrivateMessageChannel.h"
#include "SocialChatRoom.h"
#include "SocialGroupChannel.h"
#include "Interfaces/OnlineGroupsInterface.h"
#include "SocialChatManager.generated.h"

class USocialChatRoom;
class USocialPrivateMessageChannel;
class USocialReadOnlyChatChannel;
class USocialUser;
class USocialChatChannel;

enum class ESocialChannelType : uint8;

USTRUCT()
struct FSocialChatChannelConfig
{
	GENERATED_BODY()

	FSocialChatChannelConfig() { SocialUser = nullptr; }

	FSocialChatChannelConfig(class USocialUser* InSocialUser, FString InRoomId, FText InDisplayName, TArray<USocialChatChannel*> InListenChannels = TArray<USocialChatChannel*>())
		: SocialUser(InSocialUser)
		, RoomId(InRoomId)
		, DisplayName(InDisplayName)
	{
		ListenChannels = InListenChannels;
	}

	UPROPERTY()
	TObjectPtr<USocialUser> SocialUser;

	FString RoomId;

	UPROPERTY()
	TArray<TObjectPtr<USocialChatChannel>> ListenChannels;
	FText DisplayName;
};

/** The chat manager is a fully passive construct that watches for creation of chat rooms and message activity therein */
UCLASS(Within=SocialToolkit, Config=Game)
class PARTY_API USocialChatManager : public UObject
{
	GENERATED_BODY()

public:
	static USocialChatManager* CreateChatManager(USocialToolkit& InOwnerToolkit);	
	
	USocialChatRoom* GetChatRoom(const FChatRoomId& ChannelId) const;
	virtual void GetJoinedChannels(TArray<USocialChatChannel*>& JoinedChannels) const;

	//void SendDirectMessage(const ISocialUserRef& InRecipient, const FString& InMessage);
	//void SendMessage(const USocialChatChannel& Channel, const FString& InMessage);

	//virtual bool SendMessage(const FString& InChannelName, const FString& InMessage) override;
	//virtual bool SendMessage(const ISocialUserRef& InRecipient, const FString& InMessage) override;

	//virtual void CreateChatRoom(const FChatRoomId& RoomId, const FChatRoomConfig& InChatRoomConfig = FChatRoomConfig()) override;

	//virtual void ConfigureChatRoom(const FChatRoomId& RoomId, const FChatRoomConfig& InChatRoomConfig = FChatRoomConfig(), ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary) override;

	virtual void JoinChatRoomPublic(const FChatRoomId& RoomId, const FChatRoomConfig& InChatRoomConfig = FChatRoomConfig(), ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary);
	virtual void JoinChatRoomPrivate(const FChatRoomId& RoomId, const FChatRoomConfig& InChatRoomConfig = FChatRoomConfig(), ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary);

	virtual void ExitChatRoom(const FChatRoomId& RoomId, ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary);

	DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelCreated, USocialChatChannel&);
	virtual FOnChatChannelCreated& OnChannelCreated() const { return OnChannelCreatedEvent; }

	DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelLeft, USocialChatChannel&);
	virtual FOnChatChannelLeft& OnChannelLeft() const { return OnChannelLeftEvent; }

	/*virtual FOnSocialChannelChanged& OnChatRoomConfigured() override { return OnChatRoomConfiguredEvent; }
	virtual FOnSocialChannelChanged& OnChatRoomJoined() override { return OnChatRoomJoinedEvent; }
	virtual FOnSocialChannelChanged& OnChatRoomExited() override { return OnChatRoomExitedEvent; }*/

	// TODO - Deubanks - Move to Protected here (public version in UFortChatManager once it exists)
	virtual USocialChatChannel& CreateChatChannel(USocialUser& InRecipient);
	virtual USocialChatChannel* CreateChatChannel(FSocialChatChannelConfig& InConfig);

	DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelFocusRequested, USocialChatChannel&);
	DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelDisplayRequested, USocialChatChannel&);
	FOnChatChannelFocusRequested& OnChannelFocusRequested() const { return OnChannelFocusRequestedEvent; }
	FOnChatChannelDisplayRequested& OnChannelDisplayRequested() const { return OnChannelDisplayRequestedEvent; }

	virtual void FocusChatChannel(USocialUser& InChannelUser);
	virtual void FocusChatChannel(USocialChatChannel& InChannel);
	virtual void DisplayChatChannel(USocialChatChannel& InChannel);

	virtual TSubclassOf<USocialChatRoom> GetClassForChatRoom(ESocialChannelType Type) const;
	virtual TSubclassOf<USocialChatChannel> GetClassForPrivateMessage() const { return USocialPrivateMessageChannel::StaticClass(); }

	// @todo - don.eubanks - Maybe move this down into Fort level?
	virtual TSubclassOf<USocialChatChannel> GetClassForReadOnlyChannel() const { return USocialReadOnlyChatChannel::StaticClass(); }

	virtual bool IsChatRestricted() const;
	virtual TSubclassOf<USocialGroupChannel> GetClassForGroupChannel() const { return USocialGroupChannel::StaticClass(); }

	USocialToolkit& GetOwningToolkit() const;

	bool AreSlashCommandsEnabled() { return bEnableChatSlashCommands; }

	USocialChatChannel* GetChatRoomForType(ESocialChannelType Key);




	//----------------------------------------------------------------------
	// KIAROS GROUP MANAGEMENT, tbd channels?

public:

	virtual void GetGroupChannels(TArray<USocialGroupChannel*>& JoinedChannels) const;

	//DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelFocusRequested, USocialChatChannel&);
	//FOnChatChannelFocusRequested& OnGroupsChanged() const { return OnChannelFocusRequestedEvent; }

protected:
	virtual void InitializeGroupChannels();
	void LocalUserInitialized(USocialUser& LocalUser);
	void RefreshGroupsRequestCompleted(FGroupsResult Result);

	IOnlineGroupsPtr GetOnlineGroupInterface(ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary) const;
	USocialGroupChannel& FindOrCreateGroupChannel(IOnlineGroupsPtr InGroupInterface, const FUniqueNetId& GroupId);

	void OnGroupUpdated(const FUniqueNetId& GroupId);

	bool IsUniqueIdOfOwner(const FUniqueNetId& LocalUserId) const;

	// END KIAROS GROUP MANAGEMENT
	//----------------------------------------------------------------------

protected:
	IOnlineChatPtr GetOnlineChatInterface(ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary) const;
	virtual void InitializeChatManager();
	virtual ESocialChannelType TryChannelTypeLookupByRoomId(const FChatRoomId& RoomID);

	virtual void HandleChatRoomMessageReceived(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const TSharedRef<FChatMessage>& ChatMessage);
	virtual void HandleChatPrivateMessageReceived(const FUniqueNetId& LocalUserId, const TSharedRef<FChatMessage>& ChatMessage);

	virtual void OnChannelCreatedInternal(USocialChatChannel& CreatedChannel);
	virtual void OnChannelLeftInternal(USocialChatChannel& ChannelLeft);
private:
	TMap < ESocialChannelType, TWeakObjectPtr<USocialChatChannel>> ChannelsByType;

	USocialChatRoom& FindOrCreateRoom(const FChatRoomId& RoomId);
	USocialChatChannel& FindOrCreateChannel(USocialUser& SocialUser);
	USocialChatChannel& FindOrCreateChannel(const FText& DisplayName);

	void HandleChatRoomCreated(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	void HandleChatRoomConfigured(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	void HandleChatRoomJoinPublic(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	void HandleChatRoomJoinPrivate(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	void HandleChatRoomExit(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	void HandleChatRoomMemberJoin(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId);
	void HandleChatRoomMemberExit(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId);
	void HandleChatRoomMemberUpdate(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId);

	// Failure handlers (called by HandleXXX functions above)
	virtual void HandleChatRoomCreatedFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }
	virtual void HandleChatRoomConfiguredFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }
	virtual void HandleChatRoomJoinPublicFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }
	virtual void HandleChatRoomJoinPrivateFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }
	virtual void HandleChatRoomExitFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }

private:
	UPROPERTY()
	TMap<TWeakObjectPtr<USocialUser>, TObjectPtr<USocialPrivateMessageChannel>> DirectChannelsByTargetUser;

	UPROPERTY()
	TMap<FString, TObjectPtr<USocialChatRoom>> ChatRoomsById;

	UPROPERTY()
	TMap<FString, TObjectPtr<USocialReadOnlyChatChannel>> ReadOnlyChannelsByDisplayName;

	UPROPERTY(config)
	bool bEnableChatSlashCommands = true;
	
	UPROPERTY()
	TMap<FUniqueNetIdRepl, TObjectPtr<USocialGroupChannel>> GroupChannels;

	mutable FOnChatChannelCreated OnChannelCreatedEvent;
	mutable FOnChatChannelLeft OnChannelLeftEvent;
	mutable FOnChatChannelFocusRequested OnChannelFocusRequestedEvent;
	mutable FOnChatChannelDisplayRequested OnChannelDisplayRequestedEvent;
};