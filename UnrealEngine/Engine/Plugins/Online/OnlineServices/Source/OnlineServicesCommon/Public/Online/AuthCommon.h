// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/Auth.h"
#include "Online/OnlineComponent.h"

namespace UE::Online {

class FOnlineServicesCommon;

class ONLINESERVICESCOMMON_API FAccountInfoRegistry
{
public:
	virtual ~FAccountInfoRegistry() = default;

	TSharedPtr<FAccountInfo> Find(FPlatformUserId PlatformUserId) const;
	TSharedPtr<FAccountInfo> Find(FAccountId AccountId) const;

	TArray<TSharedRef<FAccountInfo>> GetAllAccountInfo(TFunction<bool(const TSharedRef<FAccountInfo>&)> Predicate) const;

protected:
	virtual void DoRegister(const TSharedRef<FAccountInfo>& AccountInfo);
	virtual void DoUnregister(const TSharedRef<FAccountInfo>& AccountInfo);

	mutable FRWLock IndexLock;

private:
	TMap<FPlatformUserId, TSharedRef<FAccountInfo>> AuthDataByPlatformUserId;
	TMap<FAccountId, TSharedRef<FAccountInfo>> AuthDataByOnlineAccountIdHandle;
};

class ONLINESERVICESCOMMON_API FAuthCommon : public TOnlineComponent<IAuth>
{
public:
	using Super = IAuth;

	FAuthCommon(FOnlineServicesCommon& InServices);

	// IOnlineComponent
	virtual void RegisterCommands() override;

	// IAuth
	virtual TOnlineAsyncOpHandle<FAuthLogin> Login(FAuthLogin::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthLogout> Logout(FAuthLogout::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthModifyAccountAttributes> ModifyAccountAttributes(FAuthModifyAccountAttributes::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthQueryExternalServerAuthTicket> QueryExternalServerAuthTicket(FAuthQueryExternalServerAuthTicket::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthQueryExternalAuthToken> QueryExternalAuthToken(FAuthQueryExternalAuthToken::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthQueryVerifiedAuthTicket> QueryVerifiedAuthTicket(FAuthQueryVerifiedAuthTicket::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthCancelVerifiedAuthTicket> CancelVerifiedAuthTicket(FAuthCancelVerifiedAuthTicket::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthBeginVerifiedAuthSession> BeginVerifiedAuthSession(FAuthBeginVerifiedAuthSession::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthEndVerifiedAuthSession> EndVerifiedAuthSession(FAuthEndVerifiedAuthSession::Params&& Params) override;
	virtual TOnlineResult<FAuthGetLocalOnlineUserByOnlineAccountId> GetLocalOnlineUserByOnlineAccountId(FAuthGetLocalOnlineUserByOnlineAccountId::Params&& Params) const override;
	virtual TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> GetLocalOnlineUserByPlatformUserId(FAuthGetLocalOnlineUserByPlatformUserId::Params&& Params) const override;
	virtual TOnlineResult<FAuthGetAllLocalOnlineUsers> GetAllLocalOnlineUsers(FAuthGetAllLocalOnlineUsers::Params&& Params) const override;
	virtual TOnlineEvent<void(const FAuthLoginStatusChanged&)> OnLoginStatusChanged() override;
	virtual TOnlineEvent<void(const FAuthPendingAuthExpiration&)> OnPendingAuthExpiration() override;
	virtual TOnlineEvent<void(const FAuthAccountAttributesChanged&)> OnAccountAttributesChanged() override;
	virtual bool IsLoggedIn(const FAccountId& AccountId) const override;

protected:
	virtual const FAccountInfoRegistry& GetAccountInfoRegistry() const = 0;

	TOnlineEventCallable<void(const FAuthLoginStatusChanged&)> OnAuthLoginStatusChangedEvent;
	TOnlineEventCallable<void(const FAuthPendingAuthExpiration&)> OnAuthPendingAuthExpirationEvent;
	TOnlineEventCallable<void(const FAuthAccountAttributesChanged&)> OnAuthAccountAttributesChangedEvent;
};

/* UE::Online */ }
