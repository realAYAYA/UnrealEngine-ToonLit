// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/ExternalUI.h"
#include "Online/OnlineComponent.h"

namespace UE::Online {

class FOnlineServicesCommon;

class ONLINESERVICESCOMMON_API FExternalUICommon : public TOnlineComponent<IExternalUI>
{
public:
	using Super = IExternalUI;

	FExternalUICommon(FOnlineServicesCommon& InServices);

	// TOnlineComponent
	virtual void RegisterCommands() override;

	// IExternalUI
	virtual TOnlineAsyncOpHandle<FExternalUIShowLoginUI> ShowLoginUI(FExternalUIShowLoginUI::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FExternalUIShowFriendsUI> ShowFriendsUI(FExternalUIShowFriendsUI::Params&& Params) override;

	virtual TOnlineEvent<void(const FExternalUIStatusChanged&)> OnExternalUIStatusChanged() override;
protected:
	TOnlineEventCallable<void(const FExternalUIStatusChanged&)> OnExternalUIStatusChangedEvent;
};

/* UE::Online */ }