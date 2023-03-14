// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialChatRoom.h"
#include "SocialPartyChatRoom.generated.h"

class USocialChatManager;
enum class EMemberExitedReason : uint8;

/** A multi-user chat room channel. Used for all chat situations outside of private user-to-user direct messages. */
UCLASS()
class PARTY_API USocialPartyChatRoom : public USocialChatRoom
{
	GENERATED_BODY()

public:
	virtual void Initialize(USocialUser* InSocialUser, const FChatRoomId& InChannelId, ESocialChannelType InSourceChannelType);

private:
	void HandlePartyMemberLeft(EMemberExitedReason Reason);
	void HandlePartyMemberJoined(UPartyMember& NewMember);
};