// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chat/SocialChatChannel.h"
#include "SocialPrivateMessageChannel.generated.h"

/**
 * A modified version of a chat room that only contains two participants - the current user and a private recipient of their messages.
 * This is equivalent to sending a "whisper".
 */
UCLASS()
class PARTY_API USocialPrivateMessageChannel : public USocialChatChannel
{
	GENERATED_BODY()

public:
	virtual void Initialize(USocialUser* InSocialUser, const FChatRoomId& InChannelId, ESocialChannelType InSourceChannelType) override;

	virtual bool SendMessage(const FString& InMessage) override;

private:
	void SetTargetUser(USocialUser& InTargetUser);

	/** The recipient of the current user's messages */
	UPROPERTY()
	TObjectPtr<USocialUser> TargetUser;
};