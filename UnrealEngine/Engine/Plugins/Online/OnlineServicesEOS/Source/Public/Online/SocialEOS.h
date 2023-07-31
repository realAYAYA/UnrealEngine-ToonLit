// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/SocialCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_friends_types.h"
#include "eos_ui_types.h"

namespace UE::Online {

class FOnlineServicesEOS;
	
class FSocialEOS : public FSocialCommon
{
public:
	FSocialEOS(FOnlineServicesEOS& InServices);

	virtual void Initialize() override;
	virtual void PreShutdown() override;

	virtual TOnlineAsyncOpHandle<FQueryFriends> QueryFriends(FQueryFriends::Params&& Params) override;
	virtual TOnlineResult<FGetFriends> GetFriends(FGetFriends::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FSendFriendInvite> SendFriendInvite(FSendFriendInvite::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAcceptFriendInvite> AcceptFriendInvite(FAcceptFriendInvite::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FRejectFriendInvite> RejectFriendInvite(FRejectFriendInvite::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FBlockUser> BlockUser(FBlockUser::Params&& Params) override;

protected:
	void OnEOSFriendsUpdate(FAccountId LocalAccountId, FAccountId FriendAccountId, EOS_EFriendsStatus PreviousStatus, EOS_EFriendsStatus CurrentStatus);

	TMap<FAccountId, TMap<FAccountId, TSharedRef<FFriend>>> FriendsLists;
	EOS_HFriends FriendsHandle = nullptr;
	EOS_HUI UIHandle = nullptr;

	EOS_NotificationId NotifyFriendsUpdateNotificationId = 0;
};

/* UE::Online */ }
