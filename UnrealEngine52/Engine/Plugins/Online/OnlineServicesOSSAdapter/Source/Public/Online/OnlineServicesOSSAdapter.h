// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/OnlineServicesCommon.h"

#include "OnlineIdOSSAdapter.h"

class IOnlineSubsystem;

namespace UE::Online {

class FOnlineServicesOSSAdapter : public FOnlineServicesCommon
{
public:
	using Super = FOnlineServicesCommon;

	ONLINESERVICESOSSADAPTER_API FOnlineServicesOSSAdapter(EOnlineServices InServicesType, const FString& InConfigName, FName InInstanceName, IOnlineSubsystem* InSubsystem);

	ONLINESERVICESOSSADAPTER_API virtual void RegisterComponents() override;
	ONLINESERVICESOSSADAPTER_API virtual void Initialize() override;
	virtual EOnlineServices GetServicesProvider() const override { return ServicesType; }
	virtual TOnlineResult<FGetResolvedConnectString> GetResolvedConnectString(FGetResolvedConnectString::Params&& Params) override;

	IOnlineSubsystem& GetSubsystem() const { return *Subsystem; }
	FOnlineAccountIdRegistryOSSAdapter& GetAccountIdRegistry() const { return *AccountIdRegistry; }
	FOnlineAccountIdRegistryOSSAdapter& GetAccountIdRegistry() { return *AccountIdRegistry; }

protected:
	EOnlineServices ServicesType;
	IOnlineSubsystem* Subsystem;
	FOnlineAccountIdRegistryOSSAdapter* AccountIdRegistry;
};

/* UE::Online */ }
