// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncStatusMessageService.h"

#include "IStormSyncTransportClientModule.h"
#include "IStormSyncTransportLocalEndpoint.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "StormSyncCoreUtils.h"
#include "StormSyncTransportClientLog.h"

FStormSyncStatusMessageService::FStormSyncStatusMessageService(const TSharedPtr<IStormSyncTransportLocalEndpoint, ESPMode::ThreadSafe> InLocalEndpoint)
	: LocalEndpointWeak(InLocalEndpoint)
{
}

FStormSyncStatusMessageService::~FStormSyncStatusMessageService()
{
	PendingStatusResponseHandlers.Empty();
}

void FStormSyncStatusMessageService::InitializeMessageEndpoint(FMessageEndpointBuilder& InEndpointBuilder)
{
	InEndpointBuilder
		.Handling<FStormSyncTransportStatusPing>(this, &FStormSyncStatusMessageService::HandleStatusPing)
		.Handling<FStormSyncTransportStatusRequest>(this, &FStormSyncStatusMessageService::HandleStatusRequest)
		.Handling<FStormSyncTransportStatusResponse>(this, &FStormSyncStatusMessageService::HandleStatusResponse);
}

void FStormSyncStatusMessageService::RequestStatus(const FMessageAddress& InRemoteAddress, const TArray<FName>& InPackageNames, const FOnStormSyncRequestStatusComplete& DoneDelegate)
{
	TArray<FName> LocalPackageNames = InPackageNames;
	FMessageAddress MessageAddress = InRemoteAddress;

	TWeakPtr<FStormSyncStatusMessageService, ESPMode::ThreadSafe> LocalWeakThis(SharedThis(this));
	
	FStormSyncCoreUtils::GetAvaFileDependenciesAsync(InPackageNames)
	.Then([LocalWeakThis, PackageNames = MoveTemp(LocalPackageNames), RemoteAddress = MoveTemp(MessageAddress), DoneDelegate](const TFuture<TArray<FStormSyncFileDependency>> Result)
	{
		const TArray<FStormSyncFileDependency> FileDependencies = Result.Get();
		if (FileDependencies.IsEmpty())
		{
			UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncStatusMessageService::RequestStatus - Async FileDependencies is empty, something went wrong"));
			return;
		}

		const TSharedPtr<FStormSyncStatusMessageService, ESPMode::ThreadSafe> LocalThis = LocalWeakThis.Pin();
		if (!LocalThis)
		{
			return;
		}

		const TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint = LocalThis->GetMessageEndpoint();
		if (!MessageEndpoint.IsValid())
		{
			UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncStatusMessageService::RequestStatus - Unable to get message endpoint"));
			return;
		}

		// Build sync request message that is going to be sent over the network for a specific recipient
		TUniquePtr<FStormSyncTransportStatusRequest> RequestStatusMessage(FMessageEndpoint::MakeMessage<FStormSyncTransportStatusRequest>(PackageNames, FileDependencies));
		if (!RequestStatusMessage.IsValid())
		{
			UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncStatusMessageService::RequestStatus - Push request message is invalid"));
			return;
		}

		// Store completion callback for later invocation when receiving back a status response
		LocalThis->AddResponseHandler(RequestStatusMessage->MessageId, DoneDelegate);

		UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncStatusMessageService::RequestStatus - FileDependencies: %d"), FileDependencies.Num());
		UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncStatusMessageService::RequestStatus - Message: %s"), *RequestStatusMessage->ToString());

		MessageEndpoint->Send(RequestStatusMessage.Release(), RemoteAddress);
	});
}

void FStormSyncStatusMessageService::AbortStatusRequest(const FGuid& InStatusRequestId)
{
	FScopeLock ScopeLock(&HandlersCriticalSection);
	PendingStatusResponseHandlers.Remove(InStatusRequestId);
}

void FStormSyncStatusMessageService::AddResponseHandler(const FGuid& InId, const FOnStormSyncRequestStatusComplete& InDelegate)
{
	FScopeLock ScopeLock(&HandlersCriticalSection);
	PendingStatusResponseHandlers.Add(InId, InDelegate);
}

void FStormSyncStatusMessageService::HandleStatusPing(const FStormSyncTransportStatusPing& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext)
{
	const TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint = GetMessageEndpoint();
	if (!MessageEndpoint.IsValid())
	{
		UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncStatusMessageService::HandleStatusPing - Unable to get client endpoint"));
		return;
	}

	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncStatusMessageService::HandleStatusPing - Sender: %s"), *MessageContext->GetSender().ToString());

	// Send back pong message so that this recipient knows about this editor instance
	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncStatusMessageService::HandleStatusPing - Send pong Message back to %s..."), *MessageContext->GetSender().ToString());
	FStormSyncTransportStatusPong* Message = FMessageEndpoint::MakeMessage<FStormSyncTransportStatusPong>();
	MessageEndpoint->Send(Message, MessageContext->GetSender());
}

void FStormSyncStatusMessageService::HandleStatusRequest(const FStormSyncTransportStatusRequest& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext)
{
	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncStatusMessageService::HandleStatusRequest - Message: %s, Sender: %s"), *InMessage.ToString(), *MessageContext->GetSender().ToString());
	const TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint = GetMessageEndpoint();
	if (!MessageEndpoint.IsValid())
	{
		UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncStatusMessageService::HandleStatusPing - Unable to get client endpoint"));
		return;
	}

	FStormSyncTransportStatusResponse* Message = FMessageEndpoint::MakeMessage<FStormSyncTransportStatusResponse>(InMessage.MessageId);

	// Compute the diff
	Message->Modifiers = FStormSyncCoreUtils::GetSyncFileModifiers(InMessage.PackageNames, InMessage.Dependencies);
	Message->bNeedsSynchronization = !Message->Modifiers.IsEmpty();

	UE_LOG(LogStormSyncClient, Display, TEXT("GetSyncFileModifiers ... - Modifiers: %d"), Message->Modifiers.Num());
	for (const FStormSyncFileModifierInfo& Modifier : Message->Modifiers)
	{
		UE_LOG(LogStormSyncClient, Display, TEXT("\t Modifier: %s"), *Modifier.ToString());
	}

	// Send back result
	MessageEndpoint->Send(Message, MessageContext->GetSender());
}

void FStormSyncStatusMessageService::HandleStatusResponse(const FStormSyncTransportStatusResponse& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext)
{
	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncStatusMessageService::HandleStatusResponse - Message: %s, Sender: %s (Id: %s)"), *InMessage.ToString(), *MessageContext->GetSender().ToString(), *InMessage.StatusRequestId.ToString());

	TArray<FStormSyncFileModifierInfo> NewModifiers;
	TArray<FStormSyncFileModifierInfo> Modifiers = InMessage.Modifiers;
	for (const FStormSyncFileModifierInfo& Modifier : Modifiers)
	{
		UE_LOG(LogStormSyncClient, Display, TEXT("\t Original Modifier: %s"), *Modifier.ToString());

		// Handle "missing" case, meaning remote is referencing the file but we are not
		// It doesn't mean we don't have the file though, check for file equality here
		if (Modifier.ModifierOperation == EStormSyncModifierOperation::Missing)
		{
			FStormSyncFileDependency FileDependency = FStormSyncCoreUtils::CreateStormSyncFile(Modifier.FileDependency.PackageName);

			// Add it to modifiers if we could not find it locally
			if (!FileDependency.IsValid())
			{
				UE_LOG(LogStormSyncClient, Display, TEXT("\t\t Adding as missing locally, Modifier: %s"), *Modifier.ToString());
				NewModifiers.Add(Modifier);
				continue;
			}

			// If state mismatched add it too
			FStormSyncFileDependency RemoteDependency = Modifier.FileDependency;

			const bool bSameSize = FileDependency.FileSize == RemoteDependency.FileSize;
			const bool bSameHash = FileDependency.FileHash == RemoteDependency.FileHash;

			if (!bSameSize || !bSameHash)
			{
				UE_LOG(LogStormSyncClient, Display, TEXT("\t\t Mismatched state locally, add it as overwrite operation. Modifier: %s"), *Modifier.ToString());

				// Mismatched state, change modifier operation to overwrite
				NewModifiers.Add(FStormSyncFileModifierInfo(EStormSyncModifierOperation::Overwrite, RemoteDependency));
			}
		}
		else
		{
			NewModifiers.Add(Modifier);
		}
	}

	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncStatusMessageService::HandleStatusResponse - NewModifiers: %d"), NewModifiers.Num());
	for (const FStormSyncFileModifierInfo& Modifier : NewModifiers)
	{
		UE_LOG(LogStormSyncClient, Display, TEXT("\t New Modifier: %s"), *Modifier.ToString());
	}

	{
		FScopeLock ScopeLock(&HandlersCriticalSection);
		const FOnStormSyncRequestStatusComplete* ResponseHandler = PendingStatusResponseHandlers.Find(InMessage.StatusRequestId);
		if (ResponseHandler && ResponseHandler->IsBound())
		{
			ResponseHandler->Execute(MakeShared<FStormSyncTransportStatusResponse>(InMessage));
			PendingStatusResponseHandlers.Remove(InMessage.StatusRequestId);
		}
	}
}

TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> FStormSyncStatusMessageService::GetMessageEndpoint() const
{
	if (const TSharedPtr<IStormSyncTransportLocalEndpoint> LocalEndpoint = GetLocalEndpoint())
	{
		return LocalEndpoint->GetMessageEndpoint();	
	}
	
	return nullptr;
}

TSharedPtr<IStormSyncTransportLocalEndpoint> FStormSyncStatusMessageService::GetLocalEndpoint() const
{
	return LocalEndpointWeak.Pin();
}
