// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusCreateSessionCallbackProxy.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "Online/CoreOnline.h"
#include "Online.h"
#include "OnlineSessionInterfaceOculus.h"

UOculusCreateSessionCallbackProxy::UOculusCreateSessionCallbackProxy(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
	, CreateCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateCompleted))
	, StartCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartCompleted))
	, NumPublicConnections(1)
{
}

UOculusCreateSessionCallbackProxy* UOculusCreateSessionCallbackProxy::CreateSession(int32 PublicConnections, FString OculusMatchmakingPool)
{
	UOculusCreateSessionCallbackProxy* Proxy = NewObject<UOculusCreateSessionCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->NumPublicConnections = PublicConnections;
	Proxy->OculusPool = MoveTemp(OculusMatchmakingPool);
	return Proxy;
}

void UOculusCreateSessionCallbackProxy::Activate()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	auto OculusSessionInterface = Online::GetSessionInterface(OCULUS_SUBSYSTEM);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (OculusSessionInterface.IsValid())
	{
		CreateCompleteDelegateHandle = OculusSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateCompleteDelegate);

		FOnlineSessionSettings Settings;
		Settings.NumPublicConnections = NumPublicConnections;
		Settings.bShouldAdvertise = true;
		Settings.bAllowJoinInProgress = true;
		Settings.bUsesPresence = true;
		Settings.bAllowJoinViaPresence = true;

		if (!OculusPool.IsEmpty())
		{
			Settings.Set(SETTING_OCULUS_POOL, OculusPool, EOnlineDataAdvertisementType::ViaOnlineService);
		}

		OculusSessionInterface->CreateSession(0, NAME_GameSession, Settings);
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Error, TEXT("Oculus platform service not available. Skipping CreateSession."));
		OnFailure.Broadcast();
	}
}

void UOculusCreateSessionCallbackProxy::OnCreateCompleted(FName SessionName, bool bWasSuccessful)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	auto OculusSessionInterface = Online::GetSessionInterface(OCULUS_SUBSYSTEM);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (OculusSessionInterface.IsValid())
	{
		OculusSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteDelegateHandle);

		if (bWasSuccessful)
		{
			StartCompleteDelegateHandle = OculusSessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartCompleteDelegate);
			OculusSessionInterface->StartSession(NAME_GameSession);

			// OnStartCompleted will get called, nothing more to do now
			return;
		}
	}

	if (!bWasSuccessful)
	{
		OnFailure.Broadcast();
	}
}

void UOculusCreateSessionCallbackProxy::OnStartCompleted(FName SessionName, bool bWasSuccessful)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	auto OculusSessionInterface = Online::GetSessionInterface(OCULUS_SUBSYSTEM);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (OculusSessionInterface.IsValid())
	{
		OculusSessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartCompleteDelegateHandle);
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
