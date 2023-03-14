// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chat/ChatSlashCommands.h"
#include "Containers/UnrealString.h"
#include "SocialTypes.h"
#include "Chat/SocialChatChannel.h"

class ULocalPlayer;
class USocialUser;
class USocialManager;
class USocialToolkit;
class USocialChatChannel;
enum class ESocialChannelType : uint8;

class PARTY_API FChannelChangeSlashCommand : public FChatSlashCommand
{
public:
	FChannelChangeSlashCommand(const FText& CommandText, ESocialChannelType InChannelType);

	virtual void ExecuteSlashCommand(USocialUser* OptionalTargetUser) const override;
	virtual bool IsEnabled() const override;
	virtual bool CanExecuteSpacebarFromPartialTokens(const TArray<FString>& UserTextTokens) const override;
	virtual bool HasSpacebarExecuteFunctionality() const { return true; }

private:
	ESocialChannelType ChannelType;

};

class PARTY_API FPartyChannelSlashCommand : public FChannelChangeSlashCommand
{
public:
	FPartyChannelSlashCommand();
};

class PARTY_API FGlobalChannelSlashCommand : public FChannelChangeSlashCommand
{
public:
	FGlobalChannelSlashCommand();
};

class PARTY_API FTeamChannelSlashCommand : public FChannelChangeSlashCommand
{
public:
	FTeamChannelSlashCommand();
};

class PARTY_API FFounderChannelSlashCommand : public FChannelChangeSlashCommand
{
public:
	FFounderChannelSlashCommand();
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

class PARTY_API FReplySlashCommand: public FChatSlashCommand
{
public:
	FReplySlashCommand();
	virtual void Init(USocialToolkit& InToolkit) override;
	virtual void ExecuteSlashCommand(USocialUser* OptionalTargetUser) const override;
	virtual bool IsEnabled() const override;
	TWeakObjectPtr<USocialChatChannel> LastUserChannel = nullptr;

private:
	void HandleChannelCreated(USocialChatChannel& NewChannel);
	void HandleChannelLeft(USocialChatChannel& LeavingChannel);
};


