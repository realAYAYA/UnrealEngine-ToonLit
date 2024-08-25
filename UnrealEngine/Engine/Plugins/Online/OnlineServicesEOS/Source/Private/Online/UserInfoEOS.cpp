// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/UserInfoEOS.h"

#include "EOSShared.h"
#include "IEOSSDKManager.h"
#include "Online/OnlineIdEOS.h"
#include "Online/OnlineServicesEOS.h"
#include "Online/AuthEOS.h"
#include "Online/OnlineErrorEOSGS.h"

#include "eos_userinfo.h"

namespace UE::Online {

FUserInfoEOS::FUserInfoEOS(FOnlineServicesEOS& InServices)
	: FUserInfoCommon(InServices)
{
}

void FUserInfoEOS::Initialize()
{
	Super::Initialize();

	UserInfoHandle = EOS_Platform_GetUserInfoInterface(*static_cast<FOnlineServicesEOS&>(GetServices()).GetEOSPlatformHandle());
	check(UserInfoHandle != nullptr);
}

TOnlineAsyncOpHandle<FQueryUserInfo> FUserInfoEOS::QueryUserInfo(FQueryUserInfo::Params&& InParams)
{
	TOnlineAsyncOpRef<FQueryUserInfo> Op = GetJoinableOp<FQueryUserInfo>(MoveTemp(InParams));
	if (!Op->IsReady())
	{
		const FQueryUserInfo::Params& Params = Op->GetParams();

		if (Params.AccountIds.IsEmpty())
		{
			Op->SetError(Errors::InvalidParams());
			return Op->GetHandle();
		}

		for (const FAccountId TargetAccountId : Params.AccountIds)
		{
			Op->Then([this, TargetAccountId](TOnlineAsyncOp<FQueryUserInfo>& Op, TPromise<const EOS_UserInfo_QueryUserInfoCallbackInfo*>&& Promise)
			{
				const FQueryUserInfo::Params& Params = Op.GetParams();

				if (!Services.Get<FAuthEOS>()->IsLoggedIn(Params.LocalAccountId))
				{
					Op.SetError(Errors::NotLoggedIn());
					Promise.EmplaceValue();
					return;
				}

				const EOS_EpicAccountId TargetUserEasId = GetEpicAccountId(TargetAccountId);
				if (!EOS_EpicAccountId_IsValid(TargetUserEasId))
				{
					Op.SetError(Errors::InvalidParams());
					Promise.EmplaceValue();
					return;
				}

				EOS_UserInfo_QueryUserInfoOptions QueryUserInfoOptions = {};
				QueryUserInfoOptions.ApiVersion = 1;
				UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_QUERYUSERINFO_API_LATEST, 1);
				QueryUserInfoOptions.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
				QueryUserInfoOptions.TargetUserId = TargetUserEasId;

				EOS_Async(EOS_UserInfo_QueryUserInfo, UserInfoHandle, QueryUserInfoOptions, MoveTemp(Promise));
			})
			.Then([this](TOnlineAsyncOp<FQueryUserInfo>& Op, const EOS_UserInfo_QueryUserInfoCallbackInfo* CallbackInfo) mutable
			{
				if (CallbackInfo->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogOnlineServices, Warning, TEXT("EOS_UserInfo_QueryUserInfo failed with result=[%s]"), *LexToString(CallbackInfo->ResultCode));
					Op.SetError(Errors::FromEOSResult(CallbackInfo->ResultCode));
				}
			});
		}

		Op->Then([](TOnlineAsyncOp<FQueryUserInfo>& Op)
			{
				Op.SetResult({});
			});

		Op->Enqueue(GetSerialQueue());
	}
	return Op->GetHandle();
}

TOnlineResult<FGetUserInfo> FUserInfoEOS::GetUserInfo(FGetUserInfo::Params&& Params)
{
	if (!Services.Get<FAuthEOS>()->IsLoggedIn(Params.LocalAccountId))
	{
		return TOnlineResult<FGetUserInfo>(Errors::NotLoggedIn());
	}

	const EOS_EpicAccountId TargetUserEasId = GetEpicAccountId(Params.AccountId);
	if (!EOS_EpicAccountId_IsValid(TargetUserEasId))
	{
		return TOnlineResult<FGetUserInfo>(Errors::InvalidParams());
	}

	EOS_UserInfo_CopyBestDisplayNameOptions Options = {};
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYBESTDISPLAYNAME_API_LATEST, 1);
	Options.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
	Options.TargetUserId = TargetUserEasId;

	EOS_UserInfo_BestDisplayName* EosBestDisplayName;
	ON_SCOPE_EXIT
	{
		EOS_UserInfo_BestDisplayName_Release(EosBestDisplayName);
	};

	EOS_EResult EosResult = EOS_UserInfo_CopyBestDisplayName(UserInfoHandle, &Options, &EosBestDisplayName);

	if (EosResult == EOS_EResult::EOS_UserInfo_BestDisplayNameIndeterminate)
	{
		EOS_UserInfo_CopyBestDisplayNameWithPlatformOptions WithPlatformOptions = {};
		WithPlatformOptions.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYBESTDISPLAYNAMEWITHPLATFORM_API_LATEST, 1);
		WithPlatformOptions.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
		WithPlatformOptions.TargetUserId = TargetUserEasId;
		WithPlatformOptions.TargetPlatformType = EOS_OPT_Epic;

		EosResult = EOS_UserInfo_CopyBestDisplayNameWithPlatform(UserInfoHandle, &WithPlatformOptions, &EosBestDisplayName);
	}

	if(EosResult != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("EOS_UserInfo_CopyBestDisplayName failed with result=[%s]"), *LexToString(EosResult));
		return TOnlineResult<FGetUserInfo>(Errors::FromEOSResult(EosResult));
	}

	TSharedRef<FUserInfo> UserInfo = MakeShared<FUserInfo>();
	UserInfo->AccountId = Params.AccountId;
	UserInfo->DisplayName = GetBestDisplayNameStr(*EosBestDisplayName);

	return TOnlineResult<FGetUserInfo>({UserInfo});
}

/* UE::Online */ }
