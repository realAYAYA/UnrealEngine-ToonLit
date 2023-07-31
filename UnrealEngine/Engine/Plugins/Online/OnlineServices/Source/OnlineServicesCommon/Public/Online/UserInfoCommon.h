// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/UserInfo.h"
#include "Online/OnlineComponent.h"

namespace UE::Online {

class FOnlineServicesCommon;

class ONLINESERVICESCOMMON_API FUserInfoCommon : public TOnlineComponent<IUserInfo>
{
public:
	using Super = IUserInfo;

	FUserInfoCommon(FOnlineServicesCommon& InServices);

	// TOnlineComponent
	virtual void RegisterCommands() override;

	// IUserInfo
	virtual TOnlineAsyncOpHandle<FQueryUserInfo> QueryUserInfo(FQueryUserInfo::Params&& Params) override;
	virtual TOnlineResult<FGetUserInfo> GetUserInfo(FGetUserInfo::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FQueryUserAvatar> QueryUserAvatar(FQueryUserAvatar::Params&& Params) override;
	virtual TOnlineResult<FGetUserAvatar> GetUserAvatar(FGetUserAvatar::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FShowUserProfile> ShowUserProfile(FShowUserProfile::Params&& Params) override;
};

/* UE::Online */ }
