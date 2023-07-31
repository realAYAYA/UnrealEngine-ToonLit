// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusUpdateSessionCallbackProxy.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "Online/CoreOnline.h"
#include "Online.h"
#include "OnlineSessionInterfaceOculus.h"

UOculusUpdateSessionCallbackProxy::UOculusUpdateSessionCallbackProxy(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
	, UpdateCompleteDelegate(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateCompleted))
	, bShouldEnqueueInMatchmakingPool(false)
{
}

UOculusUpdateSessionCallbackProxy* UOculusUpdateSessionCallbackProxy::SetSessionEnqueue(bool bShouldEnqueueInMatchmakingPool)
{
	UOculusUpdateSessionCallbackProxy* Proxy = NewObject<UOculusUpdateSessionCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->bShouldEnqueueInMatchmakingPool = bShouldEnqueueInMatchmakingPool;
	return Proxy;
}

void UOculusUpdateSessionCallbackProxy::Activate()
{
	auto OculusSessionInterface = Online::GetSessionInterface(OCULUS_SUBSYSTEM);

	if (OculusSessionInterface.IsValid())
	{
		UpdateCompleteDelegateHandle = OculusSessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(UpdateCompleteDelegate);

		FOnlineSessionSettings Settings;
		Settings.bShouldAdvertise = bShouldEnqueueInMatchmakingPool;
		OculusSessionInterface->UpdateSession(NAME_GameSession, Settings);
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Error, TEXT("Oculus platform service not available. Skipping UpdateSession."));
		OnFailure.Broadcast();
	}
}

void UOculusUpdateSessionCallbackProxy::OnUpdateCompleted(FName SessionName, bool bWasSuccessful)
{
	auto OculusSessionInterface = Online::GetSessionInterface(OCULUS_SUBSYSTEM);

	if (OculusSessionInterface.IsValid())
	{
		OculusSessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateCompleteDelegateHandle);
	}

	if (bWasSuccessful)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}

}
