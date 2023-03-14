// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/AuthCommon.h"
#include "Online/OnlineServicesEOSGSTypes.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_auth_types.h"
#include "eos_connect_types.h"

namespace UE::Online {

const int EOS_STRING_BUFFER_LENGTH = 256;
const int EOS_MAX_TOKEN_SIZE = 4096;

class FOnlineServicesEOSGS;
struct FAccountInfoEOS;
class FAccountInfoRegistryEOS;


struct FAuthLoginEASImpl
{
	static constexpr TCHAR Name[] = TEXT("LoginEASImpl");

	struct Params
	{
		FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;
		FName CredentialsType;
		FString CredentialsId;
		TVariant<FString, FExternalAuthToken> CredentialsToken;
		TArray<FString> Scopes;
	};

	struct Result
	{
		EOS_EpicAccountId EpicAccountId = nullptr;
	};
};

struct FAuthLogoutEASImpl
{
	static constexpr TCHAR Name[] = TEXT("LogoutEASImpl");

	struct Params
	{
		EOS_EpicAccountId EpicAccountId = nullptr;
	};

	struct Result
	{
	};
};

struct FAuthGetExternalAuthTokenImpl
{
	static constexpr TCHAR Name[] = TEXT("GetExternalAuthTokenImpl");

	struct Params
	{
		EOS_EpicAccountId EpicAccountId = nullptr;
	};

	struct Result
	{
		FExternalAuthToken Token;
	};
};

struct FAuthLoginConnectImpl
{
	static constexpr TCHAR Name[] = TEXT("LoginConnectImpl");

	struct Params
	{
		FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;
		FExternalAuthToken ExternalAuthToken;
	};

	struct Result
	{
		EOS_ProductUserId ProductUserId = nullptr;
	};
};

struct FAuthConnectLoginRecoveryImpl
{
	static constexpr TCHAR Name[] = TEXT("ConnectLoginRecovery");

	struct Params
	{
		/** The Epic Account ID of the local user whose connect login should be recovered. */
		EOS_EpicAccountId LocalUserId = nullptr;
	};

	struct Result
	{
	};
};

struct FAuthHandleConnectLoginStatusChangedImpl
{
	static constexpr TCHAR Name[] = TEXT("HandleConnectLoginStatusChangedImpl");

	struct Params
	{
		/** The Product User ID of the local player whose status has changed. */
		EOS_ProductUserId LocalUserId = nullptr;
		/** The status prior to the change. */
		EOS_ELoginStatus PreviousStatus = EOS_ELoginStatus::EOS_LS_NotLoggedIn;
		/** The status at the time of the notification. */
		EOS_ELoginStatus CurrentStatus = EOS_ELoginStatus::EOS_LS_NotLoggedIn;
	};

	struct Result
	{
	};
};

struct FAuthHandleConnectAuthNotifyExpirationImpl
{
	static constexpr TCHAR Name[] = TEXT("HandleConnectAuthNotifyExpirationImpl");

	struct Params
	{
		/** The Product User ID of the local player whose status has changed. */
		EOS_ProductUserId LocalUserId = nullptr;
	};

	struct Result
	{
	};
};

struct FAuthHandleEASLoginStatusChangedImpl
{
	static constexpr TCHAR Name[] = TEXT("HandleEASLoginStatusChangedImpl");

	struct Params
	{
		/** The Epic Account ID of the local user whose status has changed */
		EOS_EpicAccountId LocalUserId = nullptr;
		/** The status prior to the change */
		EOS_ELoginStatus PreviousStatus = EOS_ELoginStatus::EOS_LS_NotLoggedIn;
		/** The status at the time of the notification */
		EOS_ELoginStatus CurrentStatus = EOS_ELoginStatus::EOS_LS_NotLoggedIn;
	};

	struct Result
	{
	};
};

struct FAccountInfoEOS final : public FAccountInfo
{
	FTSTicker::FDelegateHandle RestoreLoginTimer;
	EOS_EpicAccountId EpicAccountId = nullptr;
	EOS_ProductUserId ProductUserId = nullptr;
};

class ONLINESERVICESEOSGS_API FAccountInfoRegistryEOS final : public FAccountInfoRegistry
{
public:
	using Super = FAccountInfoRegistry;

	virtual ~FAccountInfoRegistryEOS() = default;

	TSharedPtr<FAccountInfoEOS> Find(FPlatformUserId PlatformUserId) const;
	TSharedPtr<FAccountInfoEOS> Find(FAccountId AccountId) const;
	TSharedPtr<FAccountInfoEOS> Find(EOS_EpicAccountId EpicAccountId) const;
	TSharedPtr<FAccountInfoEOS> Find(EOS_ProductUserId ProductUserId) const;

	void Register(const TSharedRef<FAccountInfoEOS>& UserAuthData);
	void Unregister(FAccountId AccountId);

protected:
	virtual void DoRegister(const TSharedRef<FAccountInfo>& AccountInfo);
	virtual void DoUnregister(const TSharedRef<FAccountInfo>& AccountInfo);

private:
	TMap<EOS_EpicAccountId, TSharedRef<FAccountInfoEOS>> AuthDataByEpicAccountId;
	TMap<EOS_ProductUserId, TSharedRef<FAccountInfoEOS>> AuthDataByProductUserId;
};

class ONLINESERVICESEOSGS_API FAuthEOSGS : public FAuthCommon
{
public:
	using Super = FAuthCommon;

	FAuthEOSGS(FOnlineServicesEOSGS& InOwningSubsystem);
	virtual ~FAuthEOSGS() = default;

	// Begin IOnlineComponent
	virtual void Initialize() override;
	// End IOnlineComponent

	// Begin IAuth
	virtual void PreShutdown() override;
	virtual TOnlineAsyncOpHandle<FAuthLogin> Login(FAuthLogin::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthLogout> Logout(FAuthLogout::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthQueryVerifiedAuthTicket> QueryVerifiedAuthTicket(FAuthQueryVerifiedAuthTicket::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthCancelVerifiedAuthTicket> CancelVerifiedAuthTicket(FAuthCancelVerifiedAuthTicket::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthBeginVerifiedAuthSession> BeginVerifiedAuthSession(FAuthBeginVerifiedAuthSession::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthEndVerifiedAuthSession> EndVerifiedAuthSession(FAuthEndVerifiedAuthSession::Params&& Params) override;
	// End IAuth

	// Begin FAuthEOSGS
	virtual TFuture<FAccountId> ResolveAccountId(const FAccountId& LocalAccountId, const EOS_ProductUserId ProductUserId);
	virtual TFuture<TArray<FAccountId>> ResolveAccountIds(const FAccountId& LocalAccountId, const TArray<EOS_ProductUserId>& ProductUserIds);
	virtual TFunction<TFuture<FAccountId>(FOnlineAsyncOp& InAsyncOp, const EOS_ProductUserId& ProductUserId)> ResolveProductIdFn();
	virtual TFunction<TFuture<TArray<FAccountId>>(FOnlineAsyncOp& InAsyncOp, const TArray<EOS_ProductUserId>& ProductUserIds)> ResolveProductIdsFn();
	// End FAuthEOSGS

protected:
	// internal operations.

	TFuture<TDefaultErrorResult<FAuthLoginEASImpl>> LoginEASImpl(const FAuthLoginEASImpl::Params& LoginParams);
	TFuture<TDefaultErrorResult<FAuthLogoutEASImpl>> LogoutEASImpl(const FAuthLogoutEASImpl::Params& LogoutParams);
	TDefaultErrorResult<FAuthGetExternalAuthTokenImpl> GetExternalAuthTokenImpl(const FAuthGetExternalAuthTokenImpl::Params& Params);
	TFuture<TDefaultErrorResult<FAuthLoginConnectImpl>> LoginConnectImpl(const FAuthLoginConnectImpl::Params& LoginParams);
	TOnlineAsyncOpHandle<FAuthConnectLoginRecoveryImpl> ConnectLoginRecoveryImplOp(FAuthConnectLoginRecoveryImpl::Params&& Params);
	TOnlineAsyncOpHandle<FAuthHandleConnectLoginStatusChangedImpl> HandleConnectLoginStatusChangedImplOp(FAuthHandleConnectLoginStatusChangedImpl::Params&& Params);
	TOnlineAsyncOpHandle<FAuthHandleConnectAuthNotifyExpirationImpl> HandleConnectAuthNotifyExpirationImplOp(FAuthHandleConnectAuthNotifyExpirationImpl::Params&& Params);
	TOnlineAsyncOpHandle<FAuthHandleEASLoginStatusChangedImpl> HandleEASLoginStatusChangedImplOp(FAuthHandleEASLoginStatusChangedImpl::Params&& Params);

protected:
	// Service event handling.
	void RegisterHandlers();
	void UnregisterHandlers();
	void OnConnectLoginStatusChanged(const EOS_Connect_LoginStatusChangedCallbackInfo* Data);
	void OnConnectAuthNotifyExpiration(const EOS_Connect_AuthExpirationCallbackInfo* Data);
	void OnEASLoginStatusChanged(const EOS_Auth_LoginStatusChangedCallbackInfo* Data);

protected:
#if !UE_BUILD_SHIPPING
	static void CheckMetadata();
#endif

	virtual const FAccountInfoRegistry& GetAccountInfoRegistry() const override;

	void InitializeConnectLoginRecoveryTimer(const TSharedRef<FAccountInfoEOS>& UserAuthData);

	static FAccountId CreateAccountId(const EOS_ProductUserId ProductUserId);

	EOS_HAuth AuthHandle = nullptr;
	EOS_HConnect ConnectHandle = nullptr;
	EOSEventRegistrationPtr OnConnectLoginStatusChangedEOSEventRegistration;
	EOSEventRegistrationPtr OnConnectAuthNotifyExpirationEOSEventRegistration;
	FAccountInfoRegistryEOS AccountInfoRegistryEOS;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FAuthLoginEASImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthLoginEASImpl::Params, PlatformUserId),
	ONLINE_STRUCT_FIELD(FAuthLoginEASImpl::Params, CredentialsType),
	ONLINE_STRUCT_FIELD(FAuthLoginEASImpl::Params, CredentialsId),
	ONLINE_STRUCT_FIELD(FAuthLoginEASImpl::Params, CredentialsToken),
	ONLINE_STRUCT_FIELD(FAuthLoginEASImpl::Params, Scopes)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthLoginEASImpl::Result)
	ONLINE_STRUCT_FIELD(FAuthLoginEASImpl::Result, EpicAccountId)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthLogoutEASImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthLogoutEASImpl::Params, EpicAccountId)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthLogoutEASImpl::Result)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthGetExternalAuthTokenImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthGetExternalAuthTokenImpl::Params, EpicAccountId)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthGetExternalAuthTokenImpl::Result)
	ONLINE_STRUCT_FIELD(FAuthGetExternalAuthTokenImpl::Result, Token)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthLoginConnectImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthLoginConnectImpl::Params, PlatformUserId),
	ONLINE_STRUCT_FIELD(FAuthLoginConnectImpl::Params, ExternalAuthToken)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthLoginConnectImpl::Result)
	ONLINE_STRUCT_FIELD(FAuthLoginConnectImpl::Result, ProductUserId)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthConnectLoginRecoveryImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthConnectLoginRecoveryImpl::Params, LocalUserId)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthConnectLoginRecoveryImpl::Result)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthHandleConnectLoginStatusChangedImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthHandleConnectLoginStatusChangedImpl::Params, LocalUserId),
	ONLINE_STRUCT_FIELD(FAuthHandleConnectLoginStatusChangedImpl::Params, PreviousStatus),
	ONLINE_STRUCT_FIELD(FAuthHandleConnectLoginStatusChangedImpl::Params, CurrentStatus)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthHandleConnectLoginStatusChangedImpl::Result)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthHandleConnectAuthNotifyExpirationImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthHandleConnectAuthNotifyExpirationImpl::Params, LocalUserId)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthHandleConnectAuthNotifyExpirationImpl::Result)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthHandleEASLoginStatusChangedImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthHandleEASLoginStatusChangedImpl::Params, LocalUserId),
	ONLINE_STRUCT_FIELD(FAuthHandleEASLoginStatusChangedImpl::Params, PreviousStatus),
	ONLINE_STRUCT_FIELD(FAuthHandleEASLoginStatusChangedImpl::Params, CurrentStatus)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthHandleEASLoginStatusChangedImpl::Result)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAccountInfoEOS)
	ONLINE_STRUCT_FIELD(FAccountInfoEOS, AccountId),
	ONLINE_STRUCT_FIELD(FAccountInfoEOS, PlatformUserId),
	ONLINE_STRUCT_FIELD(FAccountInfoEOS, LoginStatus),
	ONLINE_STRUCT_FIELD(FAccountInfoEOS, Attributes),
	ONLINE_STRUCT_FIELD(FAccountInfoEOS, EpicAccountId),
	ONLINE_STRUCT_FIELD(FAccountInfoEOS, ProductUserId)
END_ONLINE_STRUCT_META()

/* Meta*/ }

/* UE::Online */ }
