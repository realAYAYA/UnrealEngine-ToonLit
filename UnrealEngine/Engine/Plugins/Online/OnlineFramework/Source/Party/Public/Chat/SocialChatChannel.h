// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "User/ISocialUserList.h"
#include "User/SocialUser.h"
#include "SocialChatMessage.h"
#include "SocialChatChannel.generated.h"

UENUM(BlueprintType)
enum class ESocialChannelType : uint8
{
	General,
	Founder,
	Party,
	Team,
	System,
	Private
};

/** Base SocialCore chat channel class (partial ISocialChatChannel implementation) */
UCLASS(Abstract, Within=SocialChatManager)
class PARTY_API USocialChatChannel : public UObject
{
	GENERATED_BODY()

public:
	USocialChatChannel() {}

	DECLARE_EVENT_OneParam(USocialChatChannel, FOnChannelUserChanged, USocialUser&);
	virtual FOnChannelUserChanged& OnUserJoinedChannel() const { return OnUserJoinedEvent; }
	virtual FOnChannelUserChanged& OnUserLeftChannel() const { return OnUserLeftEvent; }
	virtual FOnChannelUserChanged& OnChannelUserChanged() const { return OnUserChangedEvent; }

	DECLARE_EVENT_OneParam(USocialChatChannel, FOnMessageReceived, const FSocialChatMessageRef&);
	virtual FOnMessageReceived& OnMessageReceived() const { return OnMessageReceivedEvent; }

	DECLARE_EVENT_OneParam(USocialChatChannel, FOnChannelDisplayNameChanged, const FText&);
	virtual FOnChannelDisplayNameChanged& OnChannelDisplayNameChanged() const { return OnChannelDisplayNameChangedEvent; }

	virtual void Initialize(USocialUser* InSocialUser, const FChatRoomId& InChannelId, ESocialChannelType InSourceChannelType) PURE_VIRTUAL(USocialChatChannel::Initialize, return;);

	/**
	* Manually adds the given message to the channel's log locally. Representations of this channel on other clients will not receive the message.
	* Useful for adding custom messages that did not originate from a user.
	*/
	virtual void InjectLocalMessage(const TSharedRef<FSocialLocalChatMessage>& LocalMessage);
	virtual const FText& GetChannelDisplayName() const { return ChannelDisplayName; }
	virtual const TArray<FSocialChatMessageRef>& GetMessageHistory() const { return MessageHistory; }

	virtual void UpdateNow() {}
	virtual void SetAutoUpdatePeriod(float) {}

	/**
	* Sends a text message to all other users in this channel.
	* @return True if the message was sent successfully
	*/
	virtual bool SendMessage(const FString& Message) PURE_VIRTUAL(USocialChatChannel::SendMessage, return false;)

	void SetChannelDisplayName(const FText& InDisplayName);
	
	void NotifyUserJoinedChannel(USocialUser& InUser);
	void NotifyUserLeftChannel(USocialUser& InUser);
	void NotifyChannelUserChanged(USocialUser& InUser);
	void NotifyMessageReceived(const TSharedRef<FChatMessage>& InChatMessage);
	
	virtual void ListenToChannel(USocialChatChannel& Channel);

	virtual void HandleListenedChannelMessageReceived(const FSocialChatMessageRef& Message, USocialChatChannel* SourceChannel);

	ESocialChannelType GetChannelType() const { return ChannelType; }
	void SetChannelType(ESocialChannelType InType) { ChannelType = InType; }

	virtual bool SupportsMessageSending() const { return true; }
	
	DECLARE_EVENT_OneParam(USocialChatChannel, FOnHiddenChanged, bool);
	FOnHiddenChanged& OnHiddenChanged() { return OnHiddenChangedEvent; }
	bool GetIsHidden() const { return bIsHidden; }
	void SetIsHidden(bool InValue);

	// used by external classes to duplicate a message into a channel that didn't otherwise receive it
	void AddMirroredMessage(FSocialChatMessageRef NewMessage);
	void AddSystemMessage(const FText& MessageBody);
protected:

	IOnlineChatPtr GetChatInterface() const;
	void SanitizeMessage(FString& RawMessage) const;

	void AddMessageInternal(FSocialChatMessageRef NewMessage);
	
	FText ChannelDisplayName;
	ESocialChannelType ChannelType;

	USocialToolkit& GetOwningToolkit() const;

private:
	bool bIsHidden = false;
	FOnHiddenChanged OnHiddenChangedEvent;

	TArray<FSocialChatMessageRef> MessageHistory;

	mutable FOnChannelUserChanged OnUserJoinedEvent;
	mutable FOnChannelUserChanged OnUserLeftEvent;
	mutable FOnChannelUserChanged OnUserChangedEvent;
	mutable FOnMessageReceived OnMessageReceivedEvent;
	mutable FOnChannelDisplayNameChanged OnChannelDisplayNameChangedEvent;

};