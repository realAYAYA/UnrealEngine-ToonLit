// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ExternalUICommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_ui_types.h"

namespace UE::Online {

struct FExternalUIEOSConfig
{
	/** Is ShowLoginUI enabled? */
	bool bShowLoginUIEnabled = true;
};

class FOnlineServicesEOS;

class ONLINESERVICESEOS_API FExternalUIEOS : public FExternalUICommon
{
public:
	using Super = FExternalUICommon;

	FExternalUIEOS(FOnlineServicesEOS& InOwningSubsystem);
	virtual void Initialize() override;
	virtual void UpdateConfig() override;
	virtual void PreShutdown() override;
	virtual TOnlineAsyncOpHandle<FExternalUIShowLoginUI> ShowLoginUI(FExternalUIShowLoginUI::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FExternalUIShowFriendsUI> ShowFriendsUI(FExternalUIShowFriendsUI::Params&& Params) override;

protected:
	/** Handle to EOS UI */
	EOS_HUI UIHandle;
	/** Config */
	FExternalUIEOSConfig Config;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FExternalUIEOSConfig)
	ONLINE_STRUCT_FIELD(FExternalUIEOSConfig, bShowLoginUIEnabled)
END_ONLINE_STRUCT_META()

/* Meta */ }

/* UE::Online */ }
