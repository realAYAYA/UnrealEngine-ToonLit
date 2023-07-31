// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/PresenceCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_presence_types.h"

namespace UE::Online {

class FOnlineServicesEOS;
	
class FPresenceEOS : public FPresenceCommon
{
public:
	FPresenceEOS(FOnlineServicesEOS& InServices);

	virtual void Initialize() override;
	virtual void PreShutdown() override;

	virtual TOnlineAsyncOpHandle<FQueryPresence> QueryPresence(FQueryPresence::Params&& Params) override;
	virtual TOnlineResult<FGetCachedPresence> GetCachedPresence(FGetCachedPresence::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FUpdatePresence> UpdatePresence(FUpdatePresence::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FPartialUpdatePresence> PartialUpdatePresence(FPartialUpdatePresence::Params&& Params) override;

protected:
	/** Get a user's presence, creating entries if missing */
	TSharedRef<FUserPresence> FindOrCreatePresence(FAccountId LocalAccountId, FAccountId PresenceAccountId);
	/** Update a user's presence from EOS's current value */
	void UpdateUserPresence(FAccountId LocalAccountId, FAccountId PresenceAccountId);
protected:
	EOS_HPresence PresenceHandle = nullptr;

	TMap<FAccountId, TMap<FAccountId, TSharedRef<FUserPresence>>> PresenceLists;
	EOS_NotificationId NotifyPresenceChangedNotificationId = 0;
};

/* UE::Online */ }
