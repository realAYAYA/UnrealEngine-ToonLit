// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/ConnectivityCommon.h"

#include "Online/OnlineUtils.h"

namespace UE::Online {

FConnectivityCommon::FConnectivityCommon(FOnlineServicesCommon& InServices)
	: TOnlineComponent(TEXT("Connectivity"), InServices)
{
}

void FConnectivityCommon::RegisterCommands()
{
	RegisterCommand(&FConnectivityCommon::GetConnectionStatus);
}

TOnlineResult<FGetConnectionStatus> FConnectivityCommon::GetConnectionStatus(FGetConnectionStatus::Params&& Params)
{
	FGetConnectionStatus::Result Result;
	Result.Status = ConnectionStatus;

	return TOnlineResult<FGetConnectionStatus>(Result);
}

TOnlineEvent<void(const FConnectionStatusChanged&)> FConnectivityCommon::OnConnectionStatusChanged()
{
	return OnConnectionStatusChangedEvent;
}

/* UE::Online */}
