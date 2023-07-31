// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/ConnectivityCommon.h"

namespace UE::Online {

class FConnectivityOSSAdapter : public FConnectivityCommon
{
public:
	using Super = FConnectivityCommon;

	using FConnectivityCommon::FConnectivityCommon;

	// IOnlineComponent
	virtual void PostInitialize() override;
	virtual void PreShutdown() override;

	// IConnectivity
	virtual TOnlineResult<FGetConnectionStatus> GetConnectionStatus(FGetConnectionStatus::Params&& Params) override;

protected:
	FDelegateHandle OnConnectionStatusChangedHandle;
	TMap<FString, EOnlineServicesConnectionStatus> CurrentStatus;
};

/* UE::Online */ }
