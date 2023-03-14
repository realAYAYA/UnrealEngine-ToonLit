// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/UserInfoEOS.h"
#include "Online/OnlineAsyncOp.h"
#include "Online/OnlineIdEOS.h"
#include "Online/OnlineServicesEOS.h"
#include "Online/OnlineServicesEOSTypes.h"
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

	UserInfoHandle = EOS_Platform_GetUserInfoInterface(static_cast<FOnlineServicesEOS&>(GetServices()).GetEOSPlatformHandle());
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
				QueryUserInfoOptions.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST;
				static_assert(EOS_USERINFO_QUERYUSERINFO_API_LATEST == 1, "EOS_UserInfo_QueryUserInfoOptions updated, check new fields");
				QueryUserInfoOptions.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
				QueryUserInfoOptions.TargetUserId = TargetUserEasId;

				EOS_Async(EOS_UserInfo_QueryUserInfo, UserInfoHandle, QueryUserInfoOptions, MoveTemp(Promise));
			})
			.Then([this](TOnlineAsyncOp<FQueryUserInfo>& Op, const EOS_UserInfo_QueryUserInfoCallbackInfo* CallbackInfo) mutable
			{
				if (CallbackInfo->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Warning, TEXT("EOS_UserInfo_QueryUserInfo failed with result=[%s]"), *LexToString(CallbackInfo->ResultCode));
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

	EOS_UserInfo_CopyUserInfoOptions Options;
	Options.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST;
	static_assert(EOS_USERINFO_COPYUSERINFO_API_LATEST == 3, "EOS_UserInfo_CopyUserInfoOptions updated, check new fields");
	Options.LocalUserId = GetEpicAccountIdChecked(Params.LocalAccountId);
	Options.TargetUserId = TargetUserEasId;

	EOS_UserInfo* EosUserInfo = nullptr;
	ON_SCOPE_EXIT
	{
		EOS_UserInfo_Release(EosUserInfo);
	};

	const EOS_EResult EosResult = EOS_UserInfo_CopyUserInfo(UserInfoHandle, &Options, &EosUserInfo);
	if(EosResult != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS_UserInfo_CopyUserInfo failed with result=[%s]"), *LexToString(EosResult));
		return TOnlineResult<FGetUserInfo>(Errors::FromEOSResult(EosResult));
	}

	TSharedRef<FUserInfo> UserInfo = MakeShared<FUserInfo>();
	UserInfo->AccountId = Params.AccountId;
	UserInfo->DisplayName = UTF8_TO_TCHAR(EosUserInfo->DisplayName);

	return TOnlineResult<FGetUserInfo>({UserInfo});
}

/* UE::Online */ }
