// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/UserInfoCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_userinfo_types.h"

namespace UE::Online {

class FOnlineServicesEOS;
	
class FUserInfoEOS : public FUserInfoCommon
{
public:
	using Super = FUserInfoCommon;

	FUserInfoEOS(FOnlineServicesEOS& InServices);
	virtual ~FUserInfoEOS() = default;

	// IOnlineComponent
	virtual void Initialize() override;

	// IUserInfo
	virtual TOnlineAsyncOpHandle<FQueryUserInfo> QueryUserInfo(FQueryUserInfo::Params&& Params) override;
	virtual TOnlineResult<FGetUserInfo> GetUserInfo(FGetUserInfo::Params&& Params) override;

protected:
	EOS_HUserInfo UserInfoHandle = nullptr;
};

/* UE::Online */ }
