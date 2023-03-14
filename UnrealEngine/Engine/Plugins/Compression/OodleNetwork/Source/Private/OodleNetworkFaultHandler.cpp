// Copyright Epic Games, Inc. All Rights Reserved.


// Includes
#include "OodleNetworkFaultHandler.h"
#include "Net/Core/Connection/NetConnectionFaultRecoveryBase.h"
#include "Engine/NetConnection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OodleNetworkFaultHandler)


/**
 * EOodleNetResult
 */

#ifndef CASE_ENUM_TO_TEXT_RET
#define CASE_ENUM_TO_TEXT_RET(txt) case txt: ReturnVal = TEXT(#txt); break;
#endif

const TCHAR* LexToString(EOodleNetResult Enum)
{
	const TCHAR* ReturnVal = TEXT("::Invalid");

	switch (Enum)
	{
		FOREACH_ENUM_EOODLENETRESULT(CASE_ENUM_TO_TEXT_RET)
	}

	while (*ReturnVal != ':')
	{
		ReturnVal++;
	}

	ReturnVal += 2;

	return ReturnVal;
}


/**
 * FOodleNetworkFaultHandler
 */

void FOodleNetworkFaultHandler::InitFaultRecovery(UE::Net::FNetConnectionFaultRecoveryBase* InFaultRecovery)
{
	using namespace UE::Net;

	if (FaultRecovery == nullptr)
	{
		FaultRecovery = InFaultRecovery;

		if (FaultRecovery != nullptr && CounterIndex == INDEX_NONE)
		{
			FaultRecovery->GetFaultManager().AddResultHandlerPtr(this);

			CounterIndex = FaultRecovery->AddNewCounter();

			FaultRecovery->RegisterCounterCategory(ENetFaultCounterCategory::NetworkCorruption, CounterIndex);
		}
	}
}

UE::Net::EHandleNetResult FOodleNetworkFaultHandler::HandleNetResult(UE::Net::FNetResult&& InResult)
{
	using namespace UE::Net;
	using FNetFaultEscalationHandler = FNetConnectionFaultRecoveryBase::FNetFaultEscalationHandler;

	EHandleNetResult ReturnVal = EHandleNetResult::Handled;
	TNetResult<EOodleNetResult>* CastedResult = Cast<EOodleNetResult>(&InResult);
	EOodleNetResult OodleResult = (CastedResult != nullptr ? CastedResult->GetResult() : EOodleNetResult::Unknown);
	FEscalationCounter CounterIncrement;

	switch (OodleResult)
	{
		case EOodleNetResult::OodleDecodeFailed:
		case EOodleNetResult::OodleSerializePayloadFail:
		case EOodleNetResult::OodleBadDecompressedLength:
		{
			CounterIncrement.Counter++;

			break;
		}

		default:
		{
			ReturnVal = EHandleNetResult::NotHandled;

			break;
		}
	}


	if (ReturnVal != EHandleNetResult::NotHandled && CounterIndex != INDEX_NONE)
	{
		FEscalationCounter& FrameCounter = FaultRecovery->GetFrameCounter(CounterIndex);

		FrameCounter.AccumulateCounter(CounterIncrement);

		ReturnVal = FaultRecovery->NotifyHandledFault(MoveTemp(InResult));
	}
		
	return ReturnVal;
}

