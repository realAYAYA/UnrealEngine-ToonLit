// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/PresenceCommon.h"
#include "AuthOSSAdapter.h"
#include "OnlineSubsystemTypes.h"

class IOnlinePresence;
using IOnlinePresencePtr = TSharedPtr<IOnlinePresence>;

namespace UE::Online {

class FPresenceOSSAdapter : public FPresenceCommon
{
public:
	using Super = FPresenceCommon;

	using FPresenceCommon::FPresenceCommon;

	// IOnlineComponent
	virtual void PostInitialize() override;
	virtual void PreShutdown() override;

	// IPresence
	virtual TOnlineAsyncOpHandle<FQueryPresence> QueryPresence(FQueryPresence::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FBatchQueryPresence> BatchQueryPresence(FBatchQueryPresence::Params&& Params) override;
	virtual TOnlineResult<FGetCachedPresence> GetCachedPresence(FGetCachedPresence::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FUpdatePresence> UpdatePresence(FUpdatePresence::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FPartialUpdatePresence> PartialUpdatePresence(FPartialUpdatePresence::Params&& Params) override;

	FUniqueNetIdRef GetUniqueNetId(FAccountId AccountId) const;
	int32 GetLocalUserNum(FAccountId AccountId) const;

protected:
	const FAuthOSSAdapter* Auth;
	IOnlinePresencePtr PresenceInt;

	TSharedRef<FUserPresence> PresenceV1toV2(FOnlineUserPresence& Presence);
	TSharedRef<FOnlineUserPresence> PresenceV2toV1(FUserPresence& Presence);
};

/* UE::Online */ }
