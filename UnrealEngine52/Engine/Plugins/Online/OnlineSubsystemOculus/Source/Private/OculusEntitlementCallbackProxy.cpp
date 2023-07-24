// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusEntitlementCallbackProxy.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "OnlineSubsystemOculusTypes.h"
#include "Online.h"

UOculusEntitlementCallbackProxy::UOculusEntitlementCallbackProxy(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

UOculusEntitlementCallbackProxy* UOculusEntitlementCallbackProxy::VerifyEntitlement()
{
	UOculusEntitlementCallbackProxy* Proxy = NewObject<UOculusEntitlementCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	return Proxy;
}

void UOculusEntitlementCallbackProxy::Activate()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	auto OculusIdentityInterface = Online::GetIdentityInterface(OCULUS_SUBSYSTEM);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (OculusIdentityInterface.IsValid())
	{
		OculusIdentityInterface->GetUserPrivilege(
			*FUniqueNetIdOculus::EmptyId(),
			EUserPrivileges::CanPlay,
			IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate::CreateUObject(this, &UOculusEntitlementCallbackProxy::OnUserPrivilegeCompleteDelegate)
		);
	}
	else
	{
		UE_LOG_ONLINE_ENTITLEMENT(Warning, TEXT("Oculus platform service not available. Skipping entitlement check."));
		OnFailure.Broadcast();
	}
}

void UOculusEntitlementCallbackProxy::OnUserPrivilegeCompleteDelegate(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, uint32 Result)
{
    if (Result == static_cast<uint32>(IOnlineIdentity::EPrivilegeResults::NoFailures))
    {
		OnSuccess.Broadcast();
    }
    else
    {
		OnFailure.Broadcast();
    }
}

