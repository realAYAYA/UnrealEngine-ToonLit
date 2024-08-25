// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserTakesFunctionLibrary.h"

#include "ConcertLogGlobal.h"
#include "ConcertTakeRecorderManager.h"
#include "ConcertTakeRecorderMessages.h"
#include "ConcertTakeRecorderModule.h"
#include "ConcertTakeRecorderSynchronizationCustomization.h"
#include "IConcertSyncClient.h"
#include "IConcertSyncClientModule.h"

#include "Templates/Function.h"

bool UMultiUserTakesFunctionLibrary::GetRecordOnClientLocal()
{
	const TSharedPtr<IConcertSyncClient> ConcertSyncClient = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
	const TSharedPtr<IConcertClientSession> Session = ConcertSyncClient ? ConcertSyncClient->GetConcertClient()->GetCurrentSession() : nullptr;
	
	UE_CLOG(!Session, LogConcert, Warning, TEXT("MultiUserTakes: Cannot change local RecordOnClient state because this editor is nott in any session!"));
	return Session && GetRecordOnClient(Session->GetSessionClientEndpointId());
}

void UMultiUserTakesFunctionLibrary::SetRecordOnClientLocal(bool bNewValue)
{
	const TSharedPtr<IConcertSyncClient> ConcertSyncClient = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
	const TSharedPtr<IConcertClientSession> Session = ConcertSyncClient ? ConcertSyncClient->GetConcertClient()->GetCurrentSession() : nullptr;
	if (Session)
	{
		SetRecordOnClient(Session->GetSessionClientEndpointId(), bNewValue);
	}
	else
	{
		UE_LOG(LogConcert, Warning, TEXT("MultiUserTakes: Cannot change local RecordOnClient state because this editor is nott in any session!"));
	}
}

bool UMultiUserTakesFunctionLibrary::GetRecordOnClient(const FGuid& ClientEndpointId)
{
	using namespace UE::ConcertTakeRecorder;
	
	if (const FConcertTakeRecorderManager* Manager = FConcertTakeRecorderModule::Get().GetTakeRecorderManager())
	{
		const FConcertClientRecordSetting* Settings = Manager->FindClientRecorderSetting(ClientEndpointId);
		UE_CLOG(!Settings, LogConcert, Warning, TEXT("MultiUserTakes: Unknown client ID %s"), *ClientEndpointId.ToString());
		return Settings && Settings->Settings.bRecordOnClient;
	}
	return false;
}

void UMultiUserTakesFunctionLibrary::SetRecordOnClient(const FGuid& ClientEndpointId, bool bNewValue)
{
	using namespace UE::ConcertTakeRecorder;
	
	if (FConcertTakeRecorderManager* Manager = FConcertTakeRecorderModule::Get().GetTakeRecorderManager())
	{
		const bool bClientWasFound = Manager->EditClientSettings(
			ClientEndpointId,
			[bNewValue](FTakeRecordSettings& Settings) { Settings.bRecordOnClient = bNewValue; },
			{ [bNewValue](const FTakeRecordSettings& Settings){ return Settings.bRecordOnClient != bNewValue; } }
			);
		UE_CLOG(!bClientWasFound, LogConcert, Warning, TEXT("MultiUserTakes: Unknown client ID %s"), *ClientEndpointId.ToString());
	}
}

bool UMultiUserTakesFunctionLibrary::GetSynchronizeTakeRecorderTransactionsLocal()
{
	return GetMutableDefault<UConcertTakeSynchronization>()->bSyncTakeRecordingTransactions;
}

bool UMultiUserTakesFunctionLibrary::GetSynchronizeTakeRecorderTransactions(const FGuid& ClientEndpointId)
{
	using namespace UE::ConcertTakeRecorder;
	
	if (const FConcertTakeRecorderManager* Manager = FConcertTakeRecorderModule::Get().GetTakeRecorderManager())
	{
		const FConcertClientRecordSetting* Settings = Manager->FindClientRecorderSetting(ClientEndpointId);
		UE_CLOG(!Settings, LogConcert, Warning, TEXT("MultiUserTakes: Unknown client ID %s"), *ClientEndpointId.ToString());
		return Settings && Settings->bTakeSyncEnabled;
	}
	return false;
}

void UMultiUserTakesFunctionLibrary::SetSynchronizeTakeRecorderTransactionsLocal(bool bNewValue)
{
	UConcertTakeSynchronization* Settings = GetMutableDefault<UConcertTakeSynchronization>();
	if (Settings->bSyncTakeRecordingTransactions != bNewValue)
	{
		Settings->bSyncTakeRecordingTransactions = bNewValue;

		// Causes all the other clients to be notified. This should REALLY live in UConcertTakeSynchronization.
		FConcertTakeRecorderSynchronizationCustomization::OnSyncPropertyValueChanged().Broadcast(bNewValue);
	}
}
