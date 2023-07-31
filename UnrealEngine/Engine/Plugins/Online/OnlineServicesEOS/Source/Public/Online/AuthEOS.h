// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/AuthEOSGS.h"

#include "eos_userinfo_types.h"

namespace UE::Online {

class FOnlineServicesEOS;

class ONLINESERVICESEOS_API FAuthEOS : public FAuthEOSGS
{
public:
	using Super = FAuthEOSGS;

	FAuthEOS(FOnlineServicesEOS& InOwningSubsystem);
	virtual ~FAuthEOS() = default;

	// Begin IOnlineComponent
	virtual void Initialize() override;
	// End IOnlineComponent

	// Begin IAuth
	virtual TOnlineAsyncOpHandle<FAuthLogin> Login(FAuthLogin::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthQueryExternalServerAuthTicket> QueryExternalServerAuthTicket(FAuthQueryExternalServerAuthTicket::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FAuthQueryExternalAuthToken> QueryExternalAuthToken(FAuthQueryExternalAuthToken::Params&& Params) override;
	// End IAuth

	// Begin FAuthEOSGS
	virtual TFuture<FAccountId> ResolveAccountId(const FAccountId& LocalAccountId, const EOS_ProductUserId ProductUserId) override;
	virtual TFuture<TArray<FAccountId>> ResolveAccountIds(const FAccountId& LocalAccountId, const TArray<EOS_ProductUserId>& ProductUserIds) override;
	virtual TFunction<TFuture<FAccountId>(FOnlineAsyncOp& InAsyncOp, const EOS_ProductUserId& ProductUserId)> ResolveProductIdFn() override;
	virtual TFunction<TFuture<TArray<FAccountId>>(FOnlineAsyncOp& InAsyncOp, const TArray<EOS_ProductUserId>& ProductUserIds)> ResolveProductIdsFn() override;
	// End FAuthEOSGS

	TFuture<FAccountId> ResolveAccountId(const FAccountId& LocalAccountId, const EOS_EpicAccountId EpicAccountId);
	TFuture<TArray<FAccountId>> ResolveAccountIds(const FAccountId& LocalAccountId, const TArray<EOS_EpicAccountId>& EpicAccountIds);
	TFunction<TFuture<FAccountId>(FOnlineAsyncOp& InAsyncOp, const EOS_EpicAccountId& EpicAccountId)> ResolveEpicIdFn();
	TFunction<TFuture<TArray<FAccountId>>(FOnlineAsyncOp& InAsyncOp, const TArray<EOS_EpicAccountId>& EpicAccountIds)> ResolveEpicIdsFn();

protected:
	void ProcessSuccessfulLogin(TOnlineAsyncOp<FAuthLogin>& InAsyncOp);
	void OnEASLoginStatusChanged(FAccountId LocalAccountId, ELoginStatus PreviousStatus, ELoginStatus CurrentStatus);

	static FAccountId CreateAccountId(const EOS_EpicAccountId EpicAccountId, const EOS_ProductUserId ProductUserId);

	EOS_HUserInfo UserInfoHandle = nullptr;
};

/* UE::Online */ }
