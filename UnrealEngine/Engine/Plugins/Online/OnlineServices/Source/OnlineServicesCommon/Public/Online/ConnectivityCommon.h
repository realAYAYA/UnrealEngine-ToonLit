// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/Connectivity.h"
#include "Online/OnlineComponent.h"

namespace UE::Online {

class FOnlineServicesCommon;

class ONLINESERVICESCOMMON_API FConnectivityCommon : public TOnlineComponent<IConnectivity>
{
public:
	using Super = IConnectivity;

	FConnectivityCommon(FOnlineServicesCommon& InServices);

	// TOnlineComponent
	virtual void RegisterCommands() override;

	// IConnectivity
	virtual TOnlineResult<FGetConnectionStatus> GetConnectionStatus(FGetConnectionStatus::Params&& Params) override;
	virtual TOnlineEvent<void(const FConnectionStatusChanged&)> OnConnectionStatusChanged() override;

protected:
	EOnlineServicesConnectionStatus ConnectionStatus = EOnlineServicesConnectionStatus::NotConnected;

	TOnlineEventCallable<void(const FConnectionStatusChanged&)> OnConnectionStatusChangedEvent;
};

/* UE::Online */}