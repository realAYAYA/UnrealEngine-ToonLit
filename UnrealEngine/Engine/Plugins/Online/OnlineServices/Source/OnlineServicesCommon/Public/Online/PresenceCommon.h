// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/Presence.h"
#include "Online/OnlineComponent.h"

namespace UE::Online {

class FOnlineServicesCommon;

class ONLINESERVICESCOMMON_API FPresenceCommon : public TOnlineComponent<IPresence>
{
public:
	using Super = IPresence;

	FPresenceCommon(FOnlineServicesCommon& InServices);

	// IPresence
	virtual TOnlineAsyncOpHandle<FQueryPresence> QueryPresence(FQueryPresence::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FBatchQueryPresence> BatchQueryPresence(FBatchQueryPresence::Params&& Params) override;
	virtual TOnlineResult<FGetCachedPresence> GetCachedPresence(FGetCachedPresence::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FUpdatePresence> UpdatePresence(FUpdatePresence::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FPartialUpdatePresence> PartialUpdatePresence(FPartialUpdatePresence::Params&& Params) override;
	virtual TOnlineEvent<void(const FPresenceUpdated&)> OnPresenceUpdated() override;

protected:
	FOnlineServicesCommon& Services;

	TOnlineEventCallable<void(const FPresenceUpdated&)> OnPresenceUpdatedEvent;
};

/* UE::Online */ }
