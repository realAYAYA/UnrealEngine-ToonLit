// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/AuthEOS.h"

#include "Algo/Transform.h"
#include "Containers/StaticArray.h"
#include "EOSShared.h"
#include "IEOSSDKManager.h"
#include "Online/AuthErrors.h"
#include "Online/OnlineErrorEOSGS.h"
#include "Online/OnlineIdEOS.h"
#include "Online/OnlineServicesEOS.h"
#include "Online/OnlineUtils.h"

#include "eos_auth.h"
#include "eos_connect.h"
#include "eos_userinfo.h"

namespace UE::Online {

struct FAuthEOSLoginConfig
{
	TArray<FString> DefaultScopes;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FAuthEOSLoginConfig)
	ONLINE_STRUCT_FIELD(FAuthEOSLoginConfig, DefaultScopes)
END_ONLINE_STRUCT_META()

/* Meta*/ }


namespace
{

#define UE_ONLINE_AUTH_EOS_ACCOUNT_INFO_KEY_NAME TEXT("AccountInfoEOS")

/* anonymous */ }

FAuthEOS::FAuthEOS(FOnlineServicesEOS& InServices)
	: Super(InServices)
{
}

void FAuthEOS::Initialize()
{
	Super::Initialize();

	UserInfoHandle = EOS_Platform_GetUserInfoInterface(*static_cast<FOnlineServicesEOS&>(GetServices()).GetEOSPlatformHandle());
	check(UserInfoHandle != nullptr);
}

TOnlineAsyncOpHandle<FAuthLogin> FAuthEOS::Login(FAuthLogin::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthLogin> Op = GetOp<FAuthLogin>(MoveTemp(Params));
	// Step 1: Set up operation data.
	Op->Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const FAuthLogin::Params& Params = InAsyncOp.GetParams();
		TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.PlatformUserId);
		if (AccountInfoEOS)
		{
			InAsyncOp.SetError(Errors::Auth::AlreadyLoggedIn());
			return;
		}

		AccountInfoEOS = MakeShared<FAccountInfoEOS>();
		AccountInfoEOS->PlatformUserId = Params.PlatformUserId;
		AccountInfoEOS->LoginStatus = ELoginStatus::NotLoggedIn;

		// Set user auth data on operation.
		InAsyncOp.Data.Set<TSharedRef<FAccountInfoEOS>>(UE_ONLINE_AUTH_EOS_ACCOUNT_INFO_KEY_NAME, AccountInfoEOS.ToSharedRef());
	})
	// Step 2: Login EAS.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		TPromise<void> Promise;
		TFuture<void> Future = Promise.GetFuture();

		const FAuthLogin::Params& Params = InAsyncOp.GetParams();

		FAuthEOSLoginConfig AuthEOSLoginConfig;
		LoadConfig(AuthEOSLoginConfig, TEXT("Login"));

		FAuthLoginEASImpl::Params LoginParams;
		LoginParams.PlatformUserId = Params.PlatformUserId;
		LoginParams.CredentialsType = Params.CredentialsType;
		LoginParams.CredentialsId = Params.CredentialsId;
		LoginParams.CredentialsToken = Params.CredentialsToken;
		LoginParams.Scopes = !Params.Scopes.IsEmpty() ? Params.Scopes : AuthEOSLoginConfig.DefaultScopes;

		LoginEASImpl(LoginParams)
		.Next([this, Promise = MoveTemp(Promise), WeakOp = InAsyncOp.AsWeak()](TDefaultErrorResult<FAuthLoginEASImpl>&& LoginResult) mutable -> void
		{
			if (TSharedPtr<TOnlineAsyncOp<FAuthLogin>> Op = WeakOp.Pin())
			{
				const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(*Op, UE_ONLINE_AUTH_EOS_ACCOUNT_INFO_KEY_NAME);

				if (LoginResult.IsError())
				{
					UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::Login] Failure: LoginEASImpl %s"), *LoginResult.GetErrorValue().GetLogString());
					Op->SetError(Errors::Unknown(MoveTemp(LoginResult.GetErrorValue())));
				}
				else
				{
					// Cache EpicAccountId on successful EAS login.
					AccountInfoEOS->EpicAccountId = LoginResult.GetOkValue().EpicAccountId;
				}
			}

			Promise.EmplaceValue();
		});

		return Future;
	})
	// Step 3: Fetch external auth credentials for connect login.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const FAuthLogin::Params& Params = InAsyncOp.GetParams();
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, UE_ONLINE_AUTH_EOS_ACCOUNT_INFO_KEY_NAME);

		TPromise<FAuthLoginConnectImpl::Params> Promise;
		TFuture<FAuthLoginConnectImpl::Params> Future = Promise.GetFuture();

		TDefaultErrorResult<FAuthGetExternalAuthTokenImpl> AuthTokenResult = GetExternalAuthTokenImpl(FAuthGetExternalAuthTokenImpl::Params{AccountInfoEOS->EpicAccountId});
		if (AuthTokenResult.IsError())
		{
			UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::Login] Failure: GetExternalAuthTokenImpl %s"), *AuthTokenResult.GetErrorValue().GetLogString());
			InAsyncOp.SetError(Errors::Unknown(MoveTemp(AuthTokenResult.GetErrorValue())));

			// Failed to acquire token - logout EAS.
			LogoutEASImpl(FAuthLogoutEASImpl::Params{ AccountInfoEOS->EpicAccountId })
			.Next([Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLogoutEASImpl>&& LogoutResult) mutable -> void
			{
				if (LogoutResult.IsError())
				{
					UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::Login] Failure: LogoutEASImpl %s"), *LogoutResult.GetErrorValue().GetLogString());
				}
				Promise.EmplaceValue(FAuthLoginConnectImpl::Params{});
			});

			return Future;
		}

		Promise.EmplaceValue(FAuthLoginConnectImpl::Params{Params.PlatformUserId, MoveTemp(AuthTokenResult.GetOkValue().Token)});
		return Future;
	})
	// Step 4: Attempt connect login. On connect login failure handle logout of EAS.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp, FAuthLoginConnectImpl::Params&& LoginConnectParams)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, UE_ONLINE_AUTH_EOS_ACCOUNT_INFO_KEY_NAME);

		TPromise<void> Promise;
		TFuture<void> Future = Promise.GetFuture();

		// Attempt connect login.
		LoginConnectImpl(LoginConnectParams)
		.Next([this, AccountInfoEOS, WeakOp = InAsyncOp.AsWeak(), Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLoginConnectImpl>&& LoginResult) mutable -> void
		{
			if (TSharedPtr<TOnlineAsyncOp<FAuthLogin>> Op = WeakOp.Pin())
			{
				if (LoginResult.IsError())
				{
					UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::Login] Failure: LoginConnectImpl %s"), *LoginResult.GetErrorValue().GetLogString());
					Op->SetError(Errors::Unknown(MoveTemp(LoginResult.GetErrorValue())));

					LogoutEASImpl(FAuthLogoutEASImpl::Params{ AccountInfoEOS->EpicAccountId })
					.Next([Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLogoutEASImpl>&& LogoutResult) mutable -> void
					{
						if (LogoutResult.IsError())
						{
							UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::Login] Failure: LogoutEASImpl %s"), *LogoutResult.GetErrorValue().GetLogString());
						}
						Promise.EmplaceValue();
					});
				}
				else
				{
					// Successful login.
					AccountInfoEOS->ProductUserId = LoginResult.GetOkValue().ProductUserId;
					Promise.EmplaceValue();
				}
			}
			else
			{
				Promise.EmplaceValue();
			}
		});

		return Future;
	})
	// Step 5: Fetch dependent data.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, UE_ONLINE_AUTH_EOS_ACCOUNT_INFO_KEY_NAME);

		// Get display name
		EOS_UserInfo_CopyBestDisplayNameOptions Options = {};
		Options.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYBESTDISPLAYNAME_API_LATEST, 1);
		Options.LocalUserId = AccountInfoEOS->EpicAccountId;
		Options.TargetUserId = AccountInfoEOS->EpicAccountId;

		EOS_UserInfo_BestDisplayName* BestDisplayName;
		EOS_EResult CopyBestDisplayNameResult = EOS_UserInfo_CopyBestDisplayName(UserInfoHandle, &Options, &BestDisplayName);

		if (CopyBestDisplayNameResult == EOS_EResult::EOS_UserInfo_BestDisplayNameIndeterminate)
		{
			EOS_UserInfo_CopyBestDisplayNameWithPlatformOptions WithPlatformOptions = {};
			WithPlatformOptions.ApiVersion = 1;
			UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYBESTDISPLAYNAMEWITHPLATFORM_API_LATEST, 1);
			WithPlatformOptions.LocalUserId = AccountInfoEOS->EpicAccountId;
			WithPlatformOptions.TargetUserId = AccountInfoEOS->EpicAccountId;
			WithPlatformOptions.TargetPlatformType = EOS_OPT_Epic;

			CopyBestDisplayNameResult = EOS_UserInfo_CopyBestDisplayNameWithPlatform(UserInfoHandle, &WithPlatformOptions, &BestDisplayName);
		}

		if (CopyBestDisplayNameResult == EOS_EResult::EOS_Success)
		{
			AccountInfoEOS->Attributes.Emplace(AccountAttributeData::DisplayName, *GetBestDisplayNameStr(*BestDisplayName));
			EOS_UserInfo_BestDisplayName_Release(BestDisplayName);
		}
		else
		{
			FOnlineError CopyUserInfoError(Errors::FromEOSResult(CopyBestDisplayNameResult));
			UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::Login] Failure: EOS_UserInfo_CopyBestDisplayName %s"), *CopyUserInfoError.GetLogString());
			InAsyncOp.SetError(Errors::Unknown(MoveTemp(CopyUserInfoError)));

			TPromise<void> Promise;
			TFuture<void> Future = Promise.GetFuture();

			LogoutEASImpl(FAuthLogoutEASImpl::Params{ AccountInfoEOS->EpicAccountId })
			.Next([Promise = MoveTemp(Promise)](TDefaultErrorResult<FAuthLogoutEASImpl>&& LogoutResult) mutable -> void
			{
				if (LogoutResult.IsError())
				{
					UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::Login] Failure: LogoutEASImpl %s"), *LogoutResult.GetErrorValue().GetLogString());
				}
				Promise.EmplaceValue();
			});

			return Future;
		}

		return MakeFulfilledPromise<void>().GetFuture();
	})
	// Step 6: bookkeeping and notifications.
	.Then([this](TOnlineAsyncOp<FAuthLogin>& InAsyncOp)
	{
		const TSharedRef<FAccountInfoEOS>& AccountInfoEOS = GetOpDataChecked<TSharedRef<FAccountInfoEOS>>(InAsyncOp, UE_ONLINE_AUTH_EOS_ACCOUNT_INFO_KEY_NAME);
		AccountInfoEOS->LoginStatus = ELoginStatus::LoggedIn;
		AccountInfoEOS->AccountId = CreateAccountId(AccountInfoEOS->EpicAccountId, AccountInfoEOS->ProductUserId);
		AccountInfoRegistryEOS.Register(AccountInfoEOS);

		UE_LOG(LogOnlineServices, Log, TEXT("[FAuthEOS::Login] Successfully logged in as [%s]"), *ToLogString(AccountInfoEOS->AccountId));
		OnAuthLoginStatusChangedEvent.Broadcast(FAuthLoginStatusChanged{ AccountInfoEOS, AccountInfoEOS->LoginStatus });
		InAsyncOp.SetResult(FAuthLogin::Result{AccountInfoEOS});
	})
	.Enqueue(GetSerialQueue());

	return Op->GetHandle();
}

template <typename IdType>
TFuture<FAccountId> ResolveAccountIdImpl(FAuthEOS& AuthEOS, const FAccountId& LocalAccountId, const IdType InId)
{
	TPromise<FAccountId> Promise;
	TFuture<FAccountId> Future = Promise.GetFuture();

	AuthEOS.ResolveAccountIds(LocalAccountId, { InId }).Next([Promise = MoveTemp(Promise)](const TArray<FAccountId>& AccountIds) mutable
	{
		FAccountId Result;
		if (AccountIds.Num() == 1)
		{
			Result = AccountIds[0];
		}
		Promise.SetValue(Result);
	});

	return Future;
}

TOnlineAsyncOpHandle<FAuthQueryExternalServerAuthTicket> FAuthEOS::QueryExternalServerAuthTicket(FAuthQueryExternalServerAuthTicket::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthQueryExternalServerAuthTicket> Op = GetJoinableOp<FAuthQueryExternalServerAuthTicket>(MoveTemp(Params));
	if (!Op->IsReady())
	{
		Op->Then([this](TOnlineAsyncOp<FAuthQueryExternalServerAuthTicket>& InAsyncOp)
		{
			const FAuthQueryExternalServerAuthTicket::Params& Params = InAsyncOp.GetParams();
			TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.LocalAccountId);
			if (!AccountInfoEOS)
			{
				InAsyncOp.SetError(Errors::InvalidParams());
				return;
			}

			EOS_Auth_CopyUserAuthTokenOptions CopyUserAuthTokenOptions = {};
			CopyUserAuthTokenOptions.ApiVersion = 1;
			UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST, 1);

			EOS_Auth_Token* AuthToken = nullptr;

			EOS_EResult Result = EOS_Auth_CopyUserAuthToken(AuthHandle, &CopyUserAuthTokenOptions, AccountInfoEOS->EpicAccountId, &AuthToken);
			if (Result == EOS_EResult::EOS_Success)
			{
				ON_SCOPE_EXIT
				{
					EOS_Auth_Token_Release(AuthToken);
				};

				FExternalServerAuthTicket ExternalServerAuthTicket;
				ExternalServerAuthTicket.Type = ExternalLoginType::Epic;
				ExternalServerAuthTicket.Data = UTF8_TO_TCHAR(AuthToken->AccessToken);
				InAsyncOp.SetResult(FAuthQueryExternalServerAuthTicket::Result{ MoveTemp(ExternalServerAuthTicket) });
			}
			else
			{
				InAsyncOp.SetError(Errors::FromEOSResult(Result));
			}
		})
		.Enqueue(GetSerialQueue());
	}
	return Op->GetHandle();
}

TOnlineAsyncOpHandle<FAuthQueryExternalAuthToken> FAuthEOS::QueryExternalAuthToken(FAuthQueryExternalAuthToken::Params&& Params)
{
	TOnlineAsyncOpRef<FAuthQueryExternalAuthToken> Op = GetJoinableOp<FAuthQueryExternalAuthToken>(MoveTemp(Params));
	if (!Op->IsReady())
	{
		Op->Then([this](TOnlineAsyncOp<FAuthQueryExternalAuthToken>& InAsyncOp)
		{
			const FAuthQueryExternalAuthToken::Params& Params = InAsyncOp.GetParams();
			TSharedPtr<FAccountInfoEOS> AccountInfoEOS = AccountInfoRegistryEOS.Find(Params.LocalAccountId);
			if (!AccountInfoEOS)
			{
				InAsyncOp.SetError(Errors::InvalidParams());
				return;
			}

			// The primary external auth method is an id token.
			if (Params.Method == EExternalAuthTokenMethod::Primary)
			{
				TDefaultErrorResult<FAuthGetExternalAuthTokenImpl> AuthTokenResult = GetExternalAuthTokenImpl(FAuthGetExternalAuthTokenImpl::Params{ AccountInfoEOS->EpicAccountId });
				if (AuthTokenResult.IsError())
				{
					UE_LOG(LogOnlineServices, Warning, TEXT("[FAuthEOS::QueryExternalAuthToken] Failure: GetExternalAuthTokenImpl %s"), *AuthTokenResult.GetErrorValue().GetLogString());
					InAsyncOp.SetError(Errors::Unknown(MoveTemp(AuthTokenResult.GetErrorValue())));
					return;
				}

				InAsyncOp.SetResult(FAuthQueryExternalAuthToken::Result{ MoveTemp(AuthTokenResult.GetOkValue().Token) });
			}
			// The secondary external auth method is an EAS refresh token.
			else if (Params.Method == EExternalAuthTokenMethod::Secondary)
			{
				EOS_Auth_CopyUserAuthTokenOptions CopyUserAuthTokenOptions = {};
				CopyUserAuthTokenOptions.ApiVersion = 1;
				UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST, 1);

				EOS_Auth_Token* AuthToken = nullptr;

				EOS_EResult Result = EOS_Auth_CopyUserAuthToken(AuthHandle, &CopyUserAuthTokenOptions, AccountInfoEOS->EpicAccountId, &AuthToken);
				if (Result == EOS_EResult::EOS_Success)
				{
					ON_SCOPE_EXIT
					{
						EOS_Auth_Token_Release(AuthToken);
					};

					FExternalAuthToken ExternalAuthToken;
					ExternalAuthToken.Type = ExternalLoginType::Epic;
					ExternalAuthToken.Data = UTF8_TO_TCHAR(AuthToken->RefreshToken);
					InAsyncOp.SetResult(FAuthQueryExternalAuthToken::Result{ MoveTemp(ExternalAuthToken) });
				}
				else
				{
					InAsyncOp.SetError(Errors::FromEOSResult(Result));
				}
			}
			else
			{
				InAsyncOp.SetError(Errors::InvalidParams());
			}
		})
		.Enqueue(GetSerialQueue());
	}

	return Op->GetHandle();
}

TFuture<FAccountId> FAuthEOS::ResolveAccountId(const FAccountId& LocalAccountId, const EOS_EpicAccountId EpicAccountId)
{
	return ResolveAccountIdImpl(*this, LocalAccountId, EpicAccountId);
}

TFuture<FAccountId> FAuthEOS::ResolveAccountId(const FAccountId& LocalAccountId, const EOS_ProductUserId ProductUserId)
{
	return ResolveAccountIdImpl(*this, LocalAccountId, ProductUserId);
}

using FEpicAccountIdStrBuffer = TStaticArray<char, EOS_EPICACCOUNTID_MAX_LENGTH + 1>;

TFuture<TArray<FAccountId>> FAuthEOS::ResolveAccountIds(const FAccountId& LocalAccountId, const TArray<EOS_EpicAccountId>& InEpicAccountIds)
{
	// Search for all the account id's
	TArray<FAccountId> AccountIdHandles;
	AccountIdHandles.Reserve(InEpicAccountIds.Num());
	TArray<EOS_EpicAccountId> MissingEpicAccountIds;
	MissingEpicAccountIds.Reserve(InEpicAccountIds.Num());
	for (const EOS_EpicAccountId EpicAccountId : InEpicAccountIds)
	{
		if (!EOS_EpicAccountId_IsValid(EpicAccountId))
		{
			return MakeFulfilledPromise<TArray<FAccountId>>().GetFuture();
		}

		FAccountId Found = FindAccountId(EpicAccountId);
		if (!Found.IsValid())
		{
			MissingEpicAccountIds.Emplace(EpicAccountId);
		}
		AccountIdHandles.Emplace(MoveTemp(Found));
	}
	if (MissingEpicAccountIds.IsEmpty())
	{
		// We have them all, so we can just return
		return MakeFulfilledPromise<TArray<FAccountId>>(MoveTemp(AccountIdHandles)).GetFuture();
	}

	// If we failed to find all the handles, we need to query, which requires a valid LocalAccountId
	// Note this is unavailable on Dedicated Servers as well, unlike EOS_Connect_QueryProductUserIdMappings
	if (!ValidateOnlineId(LocalAccountId))
	{
		checkNoEntry();
		return MakeFulfilledPromise<TArray<FAccountId>>().GetFuture();
	}

	TPromise<TArray<FAccountId>> Promise;
	TFuture<TArray<FAccountId>> Future = Promise.GetFuture();

	TArray<FEpicAccountIdStrBuffer> EpicAccountIdStrsToQuery;
	EpicAccountIdStrsToQuery.Reserve(MissingEpicAccountIds.Num());
	for (const EOS_EpicAccountId EpicAccountId : MissingEpicAccountIds)
	{
		FEpicAccountIdStrBuffer& EpicAccountIdStr = EpicAccountIdStrsToQuery.Emplace_GetRef();
		int32_t BufferSize = sizeof(EpicAccountIdStr);
		if (!EOS_EpicAccountId_IsValid(EpicAccountId) ||
			EOS_EpicAccountId_ToString(EpicAccountId, EpicAccountIdStr.GetData(), &BufferSize) != EOS_EResult::EOS_Success)
		{
			checkNoEntry();
			return MakeFulfilledPromise<TArray<FAccountId>>().GetFuture();
		}
	}

	TArray<const char*> EpicAccountIdStrPtrs;
	Algo::Transform(EpicAccountIdStrsToQuery, EpicAccountIdStrPtrs, [](const FEpicAccountIdStrBuffer& Str) { return &Str[0]; });

	EOS_Connect_QueryExternalAccountMappingsOptions Options = {};
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST, 1);
	Options.LocalUserId = GetProductUserIdChecked(LocalAccountId);
	Options.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
	Options.ExternalAccountIds = (const char**)EpicAccountIdStrPtrs.GetData();
	Options.ExternalAccountIdCount = EpicAccountIdStrPtrs.Num();

	EOS_Async(EOS_Connect_QueryExternalAccountMappings, ConnectHandle, Options,
	[this, WeakThis = AsWeak(), InEpicAccountIds, Promise = MoveTemp(Promise)](const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data) mutable -> void
	{
		TArray<FAccountId> AccountIds;
		if (const TSharedPtr<IAuth> StrongThis = WeakThis.Pin())
		{
			AccountIds.Reserve(InEpicAccountIds.Num());
			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				EOS_Connect_GetExternalAccountMappingsOptions Options = {};
				Options.ApiVersion = 1;
				UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_GETEXTERNALACCOUNTMAPPING_API_LATEST, 1);
				Options.LocalUserId = Data->LocalUserId;
				Options.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;

				for (const EOS_EpicAccountId EpicAccountId : InEpicAccountIds)
				{
					FAccountId AccountId = FindAccountId(EpicAccountId);
					if (!AccountId.IsValid())
					{
						FEpicAccountIdStrBuffer EpicAccountIdStr;
						int32_t BufferSize = sizeof(EpicAccountIdStr);
						verify(EOS_EpicAccountId_ToString(EpicAccountId, EpicAccountIdStr.GetData(), &BufferSize) == EOS_EResult::EOS_Success);
						Options.TargetExternalUserId = &EpicAccountIdStr[0];
						const EOS_ProductUserId ProductUserId = EOS_Connect_GetExternalAccountMapping(ConnectHandle, &Options);
						AccountId = CreateAccountId(EpicAccountId, ProductUserId);
					}
					AccountIds.Emplace(MoveTemp(AccountId));
				}
			}
			else
			{
				UE_LOG(LogOnlineServices, Warning, TEXT("ResolveAccountId failed to query external mapping Result=[%s]"), *LexToString(Data->ResultCode));
			}
		}
		Promise.SetValue(MoveTemp(AccountIds));
	});

	return Future;
}

TFuture<TArray<FAccountId>> FAuthEOS::ResolveAccountIds(const FAccountId& LocalAccountId, const TArray<EOS_ProductUserId>& InProductUserIds)
{
	// Search for all the account id's
	TArray<FAccountId> AccountIdHandles;
	AccountIdHandles.Reserve(InProductUserIds.Num());
	TArray<EOS_ProductUserId> MissingProductUserIds;
	MissingProductUserIds.Reserve(InProductUserIds.Num());
	for (const EOS_ProductUserId ProductUserId : InProductUserIds)
	{
		if (!EOS_ProductUserId_IsValid(ProductUserId))
		{
			return MakeFulfilledPromise<TArray<FAccountId>>().GetFuture();
		}

		FAccountId Found = FindAccountId(ProductUserId);
		if (!Found.IsValid())
		{
			MissingProductUserIds.Emplace(ProductUserId);
		}
		AccountIdHandles.Emplace(MoveTemp(Found));
	}
	if (MissingProductUserIds.IsEmpty())
	{
		// We have them all, so we can just return
		return MakeFulfilledPromise<TArray<FAccountId>>(MoveTemp(AccountIdHandles)).GetFuture();
	}

	// If we failed to find all the handles, we need to query, which requires a valid LocalAccountId
	if (!(IsRunningDedicatedServer() || ValidateOnlineId(LocalAccountId)))
	{
		checkNoEntry();
		return MakeFulfilledPromise<TArray<FAccountId>>().GetFuture();
	}

	TPromise<TArray<FAccountId>> Promise;
	TFuture<TArray<FAccountId>> Future = Promise.GetFuture();

	EOS_Connect_QueryProductUserIdMappingsOptions Options = {};
	Options.ApiVersion = 2;
	UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_QUERYPRODUCTUSERIDMAPPINGS_API_LATEST, 2);
	Options.LocalUserId = IsRunningDedicatedServer() ? nullptr : GetProductUserIdChecked(LocalAccountId);
	Options.ProductUserIds = MissingProductUserIds.GetData();
	Options.ProductUserIdCount = MissingProductUserIds.Num();
	
	EOS_Async(EOS_Connect_QueryProductUserIdMappings, ConnectHandle, Options,
	[this, WeakThis = AsWeak(), InProductUserIds, Promise = MoveTemp(Promise)](const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data) mutable -> void
	{
		TArray<FAccountId> AccountIds;
		if (const TSharedPtr<IAuth> StrongThis = WeakThis.Pin())
		{
			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				EOS_Connect_GetProductUserIdMappingOptions Options = {};
				Options.ApiVersion = 1;
				UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_GETPRODUCTUSERIDMAPPING_API_LATEST, 1);
				Options.LocalUserId = Data->LocalUserId;
				Options.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;

				for (const EOS_ProductUserId ProductUserId : InProductUserIds)
				{
					FAccountId AccountId = FindAccountId(ProductUserId);
					if (!AccountId.IsValid())
					{
						Options.TargetProductUserId = ProductUserId;
						FEpicAccountIdStrBuffer EpicAccountIdStr;
						int32_t BufferLength = sizeof(EpicAccountIdStr);
						EOS_EpicAccountId EpicAccountId = nullptr;
						const EOS_EResult Result = EOS_Connect_GetProductUserIdMapping(ConnectHandle, &Options, EpicAccountIdStr.GetData(), &BufferLength);
						if (Result == EOS_EResult::EOS_Success)
						{
							EpicAccountId = EOS_EpicAccountId_FromString(EpicAccountIdStr.GetData());
							check(EOS_EpicAccountId_IsValid(EpicAccountId));
						}
						AccountId = CreateAccountId(EpicAccountId, ProductUserId);
					}
					AccountIds.Emplace(MoveTemp(AccountId));
				}
			}
			else
			{
				UE_LOG(LogOnlineServices, Warning, TEXT("ResolveAccountId failed to query external mapping Result=[%s]"), *LexToString(Data->ResultCode));
			}
		}
		Promise.SetValue(MoveTemp(AccountIds));
	});

	return Future;
}

template<typename ParamType>
TFunction<TFuture<FAccountId>(FOnlineAsyncOp& InAsyncOp, const ParamType& Param)> ResolveIdFnImpl(FAuthEOS* AuthEOS)
{
	return [AuthEOS](FOnlineAsyncOp& InAsyncOp, const ParamType& Param)
	{
		const FAccountId* LocalAccountIdPtr = InAsyncOp.Data.Get<FAccountId>(TEXT("LocalAccountId"));
		if (!ensure(LocalAccountIdPtr))
		{
			return MakeFulfilledPromise<FAccountId>().GetFuture();
		}
		return AuthEOS->ResolveAccountId(*LocalAccountIdPtr, Param);
	};
}
TFunction<TFuture<FAccountId>(FOnlineAsyncOp& InAsyncOp, const EOS_EpicAccountId& ProductUserId)> FAuthEOS::ResolveEpicIdFn()
{
	return ResolveIdFnImpl<EOS_EpicAccountId>(this);
}
TFunction<TFuture<FAccountId>(FOnlineAsyncOp& InAsyncOp, const EOS_ProductUserId& ProductUserId)> FAuthEOS::ResolveProductIdFn()
{
	return ResolveIdFnImpl<EOS_ProductUserId>(this);
}

template<typename ParamType>
TFunction<TFuture<TArray<FAccountId>>(FOnlineAsyncOp& InAsyncOp, const TArray<ParamType>& Param)> ResolveIdsFnImpl(FAuthEOS* AuthEOS)
{
	return [AuthEOS](FOnlineAsyncOp& InAsyncOp, const TArray<ParamType>& Param)
	{
		const FAccountId* LocalAccountIdPtr = InAsyncOp.Data.Get<FAccountId>(TEXT("LocalAccountId"));
		if (!ensure(LocalAccountIdPtr))
		{
			return MakeFulfilledPromise<TArray<FAccountId>>().GetFuture();
		}
		return AuthEOS->ResolveAccountIds(*LocalAccountIdPtr, Param);
	};
}
TFunction<TFuture<TArray<FAccountId>>(FOnlineAsyncOp& InAsyncOp, const TArray<EOS_EpicAccountId>& EpicAccountIds)> FAuthEOS::ResolveEpicIdsFn()
{
	return ResolveIdsFnImpl<EOS_EpicAccountId>(this);
}
TFunction<TFuture<TArray<FAccountId>>(FOnlineAsyncOp& InAsyncOp, const TArray<EOS_ProductUserId>& ProductUserIds)> FAuthEOS::ResolveProductIdsFn()
{
	return ResolveIdsFnImpl<EOS_ProductUserId>(this);
}

FAccountId FAuthEOS::CreateAccountId(const EOS_EpicAccountId EpicAccountId, const EOS_ProductUserId ProductUserId)
{
	return FOnlineAccountIdRegistryEOS::Get().FindOrAddAccountId(EpicAccountId, ProductUserId);
}

/* UE::Online */ }
