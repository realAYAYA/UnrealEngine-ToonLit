// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/AuthCommon.h"

#include "OnlineSubsystemTypes.h"

class IOnlineSubsystem;
class IOnlineIdentity;
using IOnlineIdentityPtr = TSharedPtr<IOnlineIdentity>;

namespace UE::Online {

class FOnlineServicesOSSAdapter;

struct FAuthHandleLoginStatusChangedImpl
{
	static constexpr TCHAR Name[] = TEXT("HandleLoginStatusChangedImpl");

	struct Params
	{
		FPlatformUserId PlatformUserId;
		FAccountId AccountId;
		ELoginStatus NewLoginStatus;
	};

	struct Result
	{
	};
};

struct FAccountInfoOSSAdapter final : public FAccountInfo
{
	FUniqueNetIdPtr UniqueNetId;
	int32 LocalUserNum = INDEX_NONE;
};

class FAccountInfoRegistryOSSAdapter final : public FAccountInfoRegistry
{
public:
	using Super = FAccountInfoRegistry;

	virtual ~FAccountInfoRegistryOSSAdapter() = default;

	TSharedPtr<FAccountInfoOSSAdapter> Find(FPlatformUserId PlatformUserId) const;
	TSharedPtr<FAccountInfoOSSAdapter> Find(FAccountId AccountId) const;

	void Register(const TSharedRef<FAccountInfoOSSAdapter>&UserAuthData);
	void Unregister(FAccountId AccountId);
};

class FAuthOSSAdapter : public FAuthCommon
{
public:
	using Super = FAuthCommon;

	using FAuthCommon::FAuthCommon;

	// IOnlineComponent
	virtual void PostInitialize() override;
	virtual void PreShutdown() override;

	// IAuth
	virtual TOnlineAsyncOpHandle<FAuthLogin> Login(FAuthLogin::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthLogout> Logout(FAuthLogout::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthQueryExternalServerAuthTicket> QueryExternalServerAuthTicket(FAuthQueryExternalServerAuthTicket::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthQueryExternalAuthToken> QueryExternalAuthToken(FAuthQueryExternalAuthToken::Params&& Params) override;

	FUniqueNetIdPtr GetUniqueNetId(FAccountId AccountId) const;
	FAccountId GetAccountId(const FUniqueNetIdRef& UniqueNetId) const;
	int32 GetLocalUserNum(FAccountId AccountId) const;

protected:
#if !UE_BUILD_SHIPPING
	static void CheckMetadata();
#endif

	virtual const FAccountInfoRegistry& GetAccountInfoRegistry() const override;

	const FOnlineServicesOSSAdapter& GetOnlineServicesOSSAdapter() const;
	FOnlineServicesOSSAdapter& GetOnlineServicesOSSAdapter();
	const IOnlineSubsystem& GetSubsystem() const;
	IOnlineIdentityPtr GetIdentityInterface() const;

	TOnlineAsyncOpHandle<FAuthHandleLoginStatusChangedImpl> HandleLoginStatusChangedImplOp(FAuthHandleLoginStatusChangedImpl::Params&& Params);

	FAccountInfoRegistryOSSAdapter AccountInfoRegistryOSSAdapter;
	FDelegateHandle OnLoginStatusChangedHandle[MAX_LOCAL_PLAYERS];
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FAuthHandleLoginStatusChangedImpl::Params)
	ONLINE_STRUCT_FIELD(FAuthHandleLoginStatusChangedImpl::Params, PlatformUserId),
	ONLINE_STRUCT_FIELD(FAuthHandleLoginStatusChangedImpl::Params, AccountId),
	ONLINE_STRUCT_FIELD(FAuthHandleLoginStatusChangedImpl::Params, NewLoginStatus)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FAuthHandleLoginStatusChangedImpl::Result)
END_ONLINE_STRUCT_META()

/* Meta*/ }

/* UE::Online */ }
