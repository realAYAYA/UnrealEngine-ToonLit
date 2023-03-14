// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chat/SocialChatChannel.h"
#include "Online/CoreOnline.h"
#include "Interfaces/OnlineGroupsInterface.h"
#include "SocialGroupChannel.generated.h"

class USocialUser;

/**
 * 
 */
UCLASS()
class PARTY_API USocialGroupChannel : public UObject
{
	GENERATED_BODY()

public:
	USocialGroupChannel();

	void Initialize(IOnlineGroupsPtr InGroupInterface, USocialUser& InSocialUser, const FUniqueNetId& InGroupId);

	void SetDisplayName(const FText& InDisplayName) { DisplayName = InDisplayName; }
	FText GetDisplayName() const { return DisplayName; }

	const TArray<USocialUser*>& GetMembers() const { return Members; }

private:
	void RefreshCompleted_GroupInfo(FGroupsResult Result);
	void RefreshCompleted_Roster(FGroupsResult Result);

private:
	UPROPERTY()
	TObjectPtr<USocialUser> SocialUser;

	UPROPERTY()
	FUniqueNetIdRepl GroupId;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	TArray<TObjectPtr<USocialUser>> Members;

	TWeakPtr<IOnlineGroups, ESPMode::ThreadSafe> GroupInterfacePtr;
};