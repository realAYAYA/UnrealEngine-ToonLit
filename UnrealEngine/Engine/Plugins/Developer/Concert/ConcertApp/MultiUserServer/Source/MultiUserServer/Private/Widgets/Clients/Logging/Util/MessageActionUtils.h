// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertTransportEvents.h"

namespace UE::MultiUserServer::MessageActionUtils
{
	/** Gets all message action names */
	TSet<FName> GetAllMessageActionNames();
	TSet<EConcertLogMessageAction> GetAllMessageActions();

	FName ConvertActionToName(EConcertLogMessageAction MessageAction);
	FString GetActionDisplayString(EConcertLogMessageAction MessageAction);
	FString GetActionDisplayString(FName MessageActionName);
};
