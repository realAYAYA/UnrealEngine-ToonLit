// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Service/GenericBarrier/DisplayClusterGenericBarrierService.h"
#include "Network/Service/GenericBarrier/DisplayClusterGenericBarrierStrings.h"
#include "Network/Barrier/DisplayClusterBarrierFactory.h"
#include "Network/Barrier/IDisplayClusterBarrier.h"
#include "Network/Packet/DisplayClusterPacketInternal.h"
#include "Network/Session/DisplayClusterSession.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"
#include "Misc/DisplayClusterTypesConverter.h"

#include "HAL/Event.h"


FDisplayClusterGenericBarrierService::FDisplayClusterGenericBarrierService()
	: FDisplayClusterService(FString("SRV_GB"))
{
}

FDisplayClusterGenericBarrierService::~FDisplayClusterGenericBarrierService()
{
	Shutdown();
}


void FDisplayClusterGenericBarrierService::Shutdown()
{
	{
		FScopeLock Lock(&BarriersCS);

		// Deactivate all barriers
		for (TPair<FString, TSharedPtr<IDisplayClusterBarrier>>& Barrier : Barriers)
		{
			Barrier.Value->Deactivate();
		}

		// Release events
		for (TPair<FString, FEvent*>& EventIt : BarrierCreationEvents)
		{
			EventIt.Value->Trigger();
			FPlatformProcess::ReturnSynchEventToPool(EventIt.Value);
		}

		BarrierCreationEvents.Empty();
	}

	return FDisplayClusterServer::Shutdown();
}

FString FDisplayClusterGenericBarrierService::GetProtocolName() const
{
	static const FString ProtocolName(DisplayClusterGenericBarrierStrings::ProtocolName);
	return ProtocolName;
}

TSharedPtr<IDisplayClusterBarrier, ESPMode::ThreadSafe> FDisplayClusterGenericBarrierService::GetBarrier(const FString& BarrierId)
{
	FScopeLock Lock(&BarriersCS);
	TSharedPtr<IDisplayClusterBarrier, ESPMode::ThreadSafe>* BarrierFound = Barriers.Find(BarrierId);
	return BarrierFound ? *BarrierFound : nullptr;
}

TSharedPtr<IDisplayClusterSession> FDisplayClusterGenericBarrierService::CreateSession(FDisplayClusterSessionInfo& SessionInfo)
{
	SessionInfo.SessionName = FString::Printf(TEXT("%s_%lu_%s_%s"),
		*GetName(),
		SessionInfo.SessionId,
		*SessionInfo.Endpoint.ToString(),
		*SessionInfo.NodeId.Get(TEXT("(na)"))
	);

	return MakeShared<FDisplayClusterSession<FDisplayClusterPacketInternal, true>>(SessionInfo, *this, *this, FDisplayClusterService::GetThreadPriority());
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionPacketHandler
//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<FDisplayClusterPacketInternal> FDisplayClusterGenericBarrierService::ProcessPacket(const TSharedPtr<FDisplayClusterPacketInternal>& Request, const FDisplayClusterSessionInfo& SessionInfo)
{
	// Check the pointer
	if (!Request.IsValid())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - Invalid request data (nullptr)"), *GetName());
		return nullptr;
	}

	// Cache session info
	SetSessionInfoCache(SessionInfo);

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - Processing packet: %s"), *GetName(), *Request->ToLogString());

	// Check protocol and type
	if (Request->GetProtocol() != DisplayClusterGenericBarrierStrings::ProtocolName || Request->GetType() != DisplayClusterGenericBarrierStrings::TypeRequest)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - Unsupported packet type: %s"), *GetName(), *Request->ToLogString());
		return nullptr;
	}

	// Initialize response packet
	TSharedPtr<FDisplayClusterPacketInternal> Response = MakeShared<FDisplayClusterPacketInternal>(Request->GetName(), DisplayClusterGenericBarrierStrings::TypeResponse, Request->GetProtocol());

	// Dispatch the packet
	if (Request->GetName().Equals(DisplayClusterGenericBarrierStrings::SyncOnBarrier::Name, ESearchCase::IgnoreCase))
	{
		// Extract parameter: barrier ID
		FString BarrierId;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgBarrierId, BarrierId);

		// Extract parameter: thread marker
		FString UniqueThreadMarker;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::SyncOnBarrier::ArgThreadMarker, UniqueThreadMarker);

		// Process command
		EBarrierControlResult CtrlResult = EBarrierControlResult::UnknownError;
		const EDisplayClusterCommResult Result = SyncOnBarrier(BarrierId, UniqueThreadMarker, CtrlResult);

		// Set response data
		Response->SetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgResult, DisplayClusterTypesConverter::template ToString((uint8)CtrlResult));
		Response->SetCommResult(Result);

		return Response;
	}
	else if (Request->GetName().Equals(DisplayClusterGenericBarrierStrings::SyncOnBarrierWithData::Name, ESearchCase::IgnoreCase))
	{
		// Extract parameter: barrier ID
		FString BarrierId;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgBarrierId, BarrierId);

		// Extract parameter: thread marker
		FString UniqueThreadMarker;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::SyncOnBarrierWithData::ArgThreadMarker, UniqueThreadMarker);

		// Extract parameter: request data
		TArray<uint8> RequestData;
		Request->GetBinArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::SyncOnBarrierWithData::ArgRequestData, RequestData);

		// Process command
		EBarrierControlResult CtrlResult = EBarrierControlResult::UnknownError;
		TArray<uint8> ResponseData;
		const EDisplayClusterCommResult Result = SyncOnBarrierWithData(BarrierId, UniqueThreadMarker, RequestData, ResponseData, CtrlResult);

		// Set response data
		Response->SetBinArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::SyncOnBarrierWithData::ArgResponseData, ResponseData);
		Response->SetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgResult, DisplayClusterTypesConverter::template ToString((uint8)CtrlResult));
		Response->SetCommResult(Result);

		return Response;
	}
	else if (Request->GetName().Equals(DisplayClusterGenericBarrierStrings::CreateBarrier::Name, ESearchCase::IgnoreCase))
	{
		// Extract parameter: barrier ID
		FString BarrierId;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgBarrierId, BarrierId);

		// Extract parameter: thread markers
		FString UniqueThreadMarkersStringified;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::CreateBarrier::ArgThreadMarkers, UniqueThreadMarkersStringified);
		TArray<FString> UniqueThreadMarkers;
		DisplayClusterHelpers::str::template StrToArray(UniqueThreadMarkersStringified, DisplayClusterStrings::common::ArrayValSeparator, UniqueThreadMarkers);

		// Extract parameter: timeout
		uint32 Timeout = 0;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::CreateBarrier::ArgTimeout, Timeout);

		// Process command
		EBarrierControlResult CtrlResult = EBarrierControlResult::UnknownError;
		const EDisplayClusterCommResult Result = CreateBarrier(BarrierId, UniqueThreadMarkers, Timeout, CtrlResult);

		// Set response data
		Response->SetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgResult, DisplayClusterTypesConverter::template ToString((uint8)CtrlResult));
		Response->SetCommResult(Result);

		return Response;
	}
	else if (Request->GetName().Equals(DisplayClusterGenericBarrierStrings::WaitUntilBarrierIsCreated::Name, ESearchCase::IgnoreCase))
	{
		// Extract parameter: barrier ID
		FString BarrierId;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgBarrierId, BarrierId);

		// Process command
		EBarrierControlResult CtrlResult = EBarrierControlResult::UnknownError;
		const EDisplayClusterCommResult Result = WaitUntilBarrierIsCreated(BarrierId, CtrlResult);

		// Set response data
		Response->SetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgResult, DisplayClusterTypesConverter::template ToString((uint8)CtrlResult));
		Response->SetCommResult(Result);

		return Response;
	}
	else if (Request->GetName().Equals(DisplayClusterGenericBarrierStrings::ReleaseBarrier::Name, ESearchCase::IgnoreCase))
	{
		// Extract parameter: barrier ID
		FString BarrierId;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgBarrierId, BarrierId);

		// Process command
		EBarrierControlResult CtrlResult = EBarrierControlResult::UnknownError;
		const EDisplayClusterCommResult Result = ReleaseBarrier(BarrierId, CtrlResult);

		// Set response data
		Response->SetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgResult, DisplayClusterTypesConverter::template ToString((uint8)CtrlResult));
		Response->SetCommResult(Result);

		return Response;
	}
	else if (Request->GetName().Equals(DisplayClusterGenericBarrierStrings::IsBarrierAvailable::Name, ESearchCase::IgnoreCase))
	{
		// Extract parameter: barrier ID
		FString BarrierId;
		Request->GetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgBarrierId, BarrierId);

		// Process command
		EBarrierControlResult CtrlResult = EBarrierControlResult::UnknownError;
		const EDisplayClusterCommResult Result = IsBarrierAvailable(BarrierId, CtrlResult);

		// Set response data
		Response->SetTextArg(DisplayClusterGenericBarrierStrings::ArgumentsDefaultCategory, DisplayClusterGenericBarrierStrings::ArgResult, DisplayClusterTypesConverter::template ToString((uint8)CtrlResult));
		Response->SetCommResult(Result);

		return Response;
	}

	// Being here means that we have no appropriate dispatch logic for this packet
	UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s - No dispatcher found for packet '%s'"), *GetName(), *Request->GetName());

	return nullptr;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolGenericBarrier
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterGenericBarrierService::CreateBarrier(const FString& BarrierId, const TArray<FString>& UniqueThreadMarkers, uint32 Timeout, EBarrierControlResult& Result)
{
	// Validate input data
	if (BarrierId.IsEmpty() || UniqueThreadMarkers.Num() < 1 || Timeout == 0)
	{
		return EDisplayClusterCommResult::WrongRequestData;
	}

	{
		FScopeLock Lock(&BarriersCS);

		// Check if exists
		if (Barriers.Contains(BarrierId))
		{
			Result = EBarrierControlResult::AlreadyExists;
			return EDisplayClusterCommResult::Ok;
		}

		// Create a new one
		TSharedPtr<IDisplayClusterBarrier, ESPMode::ThreadSafe> NewBarrier(FDisplayClusterBarrierFactory::CreateBarrier(BarrierId, UniqueThreadMarkers, Timeout));
		checkSlow(NewBarrier);

		// Activate barrier
		NewBarrier->Activate();

		// Store it
		Barriers.Emplace(BarrierId, MoveTemp(NewBarrier));

		// Notify listeners if there are any
		if (FEvent** BarrierCreatedEvent = BarrierCreationEvents.Find(BarrierId))
		{
			(*BarrierCreatedEvent)->Trigger();
		}

		Result = EBarrierControlResult::CreatedSuccessfully;
	}

	return EDisplayClusterCommResult::Ok;
}

EDisplayClusterCommResult FDisplayClusterGenericBarrierService::WaitUntilBarrierIsCreated(const FString& BarrierId, EBarrierControlResult& Result)
{
	FEvent** BarrierAvailableEvent = nullptr;

	{
		FScopeLock Lock(&BarriersCS);

		// Check if the barrier exists already
		if (Barriers.Contains(BarrierId))
		{
			Result = EBarrierControlResult::AlreadyExists;
			return EDisplayClusterCommResult::Ok;
		}
		// If no, set up a notification event
		else
		{
			// Check if notification event has been created previously
			BarrierAvailableEvent = BarrierCreationEvents.Find(BarrierId);
			if (!BarrierAvailableEvent)
			{
				// It hasn't, so create it
				BarrierAvailableEvent = &BarrierCreationEvents.Emplace(BarrierId, FPlatformProcess::GetSynchEventFromPool(true));
			}
		}
	}

	// So the barrier has not been created yet, we need to wait

	// Make sure the event is valid
	if (!BarrierAvailableEvent)
	{
		Result = EBarrierControlResult::UnknownError;
		return EDisplayClusterCommResult::InternalError;
	}

	// Wait until created
	(*BarrierAvailableEvent)->Wait();

	Result = EBarrierControlResult::AlreadyExists;
	return EDisplayClusterCommResult::Ok;
}

EDisplayClusterCommResult FDisplayClusterGenericBarrierService::IsBarrierAvailable(const FString& BarrierId, EBarrierControlResult& Result)
{
	// Validate input data
	if (BarrierId.IsEmpty())
	{
		return EDisplayClusterCommResult::WrongRequestData;
	}

	{
		FScopeLock Lock(&BarriersCS);
		Result = (Barriers.Contains(BarrierId) ? EBarrierControlResult::AlreadyExists : EBarrierControlResult::NotFound);
	}

	return EDisplayClusterCommResult::Ok;
}

EDisplayClusterCommResult FDisplayClusterGenericBarrierService::ReleaseBarrier(const FString& BarrierId, EBarrierControlResult& Result)
{
	// Validate input data
	if (BarrierId.IsEmpty())
	{
		return EDisplayClusterCommResult::WrongRequestData;
	}

	{
		FScopeLock Lock(&BarriersCS);

		if (TSharedPtr<IDisplayClusterBarrier, ESPMode::ThreadSafe>* Barrier = Barriers.Find(BarrierId))
		{
			// Deactivate barrier first because it can be used by other clients currently.
			// In this case the destructor won't be called.
			(*Barrier)->Deactivate();

			// And forget about it. Once all the clients leave the barrier, the instance will be released.
			Barriers.Remove(BarrierId);

			// We can safely release the creation event as well
			if (FEvent** BarrierCreatedEvent = BarrierCreationEvents.Find(BarrierId))
			{
				FPlatformProcess::ReturnSynchEventToPool(*BarrierCreatedEvent);
				BarrierCreationEvents.Remove(BarrierId);
			}
		}
		else
		{
			// Not exists
			Result = EBarrierControlResult::NotFound;
			return EDisplayClusterCommResult::WrongRequestData;
		}
	}

	Result = EBarrierControlResult::ReleasedSuccessfully;
	return EDisplayClusterCommResult::Ok;
}

EDisplayClusterCommResult FDisplayClusterGenericBarrierService::SyncOnBarrier(const FString& BarrierId, const FString& UniqueThreadMarker, EBarrierControlResult& Result)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SRV_GB::SyncOnBarrier);

	// Validate input data
	if (BarrierId.IsEmpty() || UniqueThreadMarker.IsEmpty())
	{
		Result = EBarrierControlResult::UnknownError;
		return EDisplayClusterCommResult::WrongRequestData;
	}

	TSharedPtr<IDisplayClusterBarrier, ESPMode::ThreadSafe>* Barrier = nullptr;

	{
		FScopeLock Lock(&BarriersCS);

		// Get barrier ptr to be able to use outside of the critical section
		// so the other clients can also access it.
		Barrier = Barriers.Find(BarrierId);
	}

	// More validation
	if (!Barrier)
	{
		// Barrier not found
		Result = EBarrierControlResult::NotFound;
		return EDisplayClusterCommResult::WrongRequestData;
	}
	else if(!(*Barrier)->IsActivated())
	{
		Result = EBarrierControlResult::UnknownError;
		return EDisplayClusterCommResult::NotAllowed;
	}

	// Sync on the barrier
	(*Barrier)->Wait(UniqueThreadMarker);

	Result = EBarrierControlResult::SynchronizedSuccessfully;
	return EDisplayClusterCommResult::Ok;
}

EDisplayClusterCommResult FDisplayClusterGenericBarrierService::SyncOnBarrierWithData(const FString& BarrierId, const FString& UniqueThreadMarker, const TArray<uint8>& RequestData, TArray<uint8>& OutResponseData, EBarrierControlResult& Result)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SRV_GB::SyncOnBarrierWithData);

	// Validate input data
	if (BarrierId.IsEmpty() || UniqueThreadMarker.IsEmpty())
	{
		Result = EBarrierControlResult::UnknownError;
		return EDisplayClusterCommResult::WrongRequestData;
	}

	TSharedPtr<IDisplayClusterBarrier, ESPMode::ThreadSafe>* Barrier = nullptr;

	{
		FScopeLock Lock(&BarriersCS);

		// Get barrier ptr to be able to use outside of the critical section
		// so the other clients can also access it.
		Barrier = Barriers.Find(BarrierId);
	}

	// More validation
	if (!Barrier)
	{
		// Barrier not found
		Result = EBarrierControlResult::NotFound;
		return EDisplayClusterCommResult::WrongRequestData;
	}
	else if (!(*Barrier)->IsActivated())
	{
		Result = EBarrierControlResult::UnknownError;
		return EDisplayClusterCommResult::NotAllowed;
	}

	// Sync on the barrier
	(*Barrier)->WaitWithData(UniqueThreadMarker, RequestData, OutResponseData);

	Result = EBarrierControlResult::SynchronizedSuccessfully;
	return EDisplayClusterCommResult::Ok;
}
