// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/UserInfoCommon.h"

#include "OnlineSubsystemTypes.h"

using IOnlineUserPtr = TSharedPtr<class IOnlineUser>;

namespace UE::Online {

class FUserInfoOSSAdapter : public FUserInfoCommon
{
public:
	using Super = FUserInfoCommon;

	using FUserInfoCommon::FUserInfoCommon;

	// IOnlineComponent
	virtual void Initialize() override;

	// IUserInfo
	virtual TOnlineAsyncOpHandle<FQueryUserInfo> QueryUserInfo(FQueryUserInfo::Params&& Params) override;
	virtual TOnlineResult<FGetUserInfo> GetUserInfo(FGetUserInfo::Params&& Params) override;

protected:
	IOnlineUserPtr UserInterface = nullptr;
};

/* UE::Online */ }
