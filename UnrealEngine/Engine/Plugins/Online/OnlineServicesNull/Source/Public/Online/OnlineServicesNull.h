// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/OnlineServicesCommon.h"

namespace UE::Online {

class ONLINESERVICESNULL_API FOnlineServicesNull : public FOnlineServicesCommon
{
public:
	using Super = FOnlineServicesCommon;

	FOnlineServicesNull(FName InInstanceName);
	virtual void RegisterComponents() override;
	virtual void Initialize() override;
	virtual TOnlineResult<FGetResolvedConnectString> GetResolvedConnectString(FGetResolvedConnectString::Params&& Params) override;
	virtual EOnlineServices GetServicesProvider() const override { return EOnlineServices::Null; }

};

/* UE::Online */ }
