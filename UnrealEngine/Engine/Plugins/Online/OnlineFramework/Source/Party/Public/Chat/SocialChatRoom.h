// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialChatChannel.h"
#include "Interfaces/OnlineChatInterface.h"
#include "SocialChatRoom.generated.h"

class USocialChatManager;

/** A multi-user chat room channel. Used for all chat situations outside of private user-to-user direct messages. */
UCLASS()
class PARTY_API USocialChatRoom : public USocialChatChannel
{
	GENERATED_BODY()

public:
	virtual void Initialize(USocialUser* InSocialUser, const FChatRoomId& InChannelId, ESocialChannelType InSourceChannelType);

	/*virtual bool HasJoinedChatRoom() const override;

	virtual bool LeaveChannel() override;*/

	virtual bool SendMessage(const FString& InMessage) override;
	const FChatRoomId& GetChatRoomId() const { return RoomId; }
	
	/*virtual void NotifyUserJoinedChannel(const ISocialUserRef& InUser) override;
	virtual void NotifyUserLeftChannel(const ISocialUserRef& InUser) override;
	virtual void NotifyChannelUserChanged(const ISocialUserRef& InUser) override;
	virtual void NotifyMessageReceived(const TSharedRef<FChatMessage>& InChatMessage) override;*/

	//virtual bool HasUnreadMessages() const override;
	virtual const FText DetermineChannelDisplayName(ESocialChannelType InSourceChannelType, const FChatRoomId& InRoomId);

private:
	void SetRoomId(const FChatRoomId& Id) { RoomId = Id; }

	FChatRoomId RoomId;
};