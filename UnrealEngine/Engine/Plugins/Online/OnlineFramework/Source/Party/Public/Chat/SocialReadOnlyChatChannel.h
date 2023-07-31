// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chat/SocialChatChannel.h"
#include "SocialReadOnlyChatChannel.generated.h"

class USocialChatManager;

/**
 * A strawman chat channel that relies exclusively on other channels messages for content, does not support sending messages
 */
UCLASS()
class PARTY_API USocialReadOnlyChatChannel : public USocialChatChannel
{
	GENERATED_BODY()

public:
	virtual void Initialize(USocialUser* InSocialUser, const FChatRoomId& InChannelId, ESocialChannelType InSourceChannelType) override;

	virtual bool SendMessage(const FString& InMessage) override;

	virtual bool SupportsMessageSending() const override { return false; }
};