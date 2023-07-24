// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"

enum class EConcertLogMessageAction : uint8;

namespace UE::MultiUserServer::MessageActionUtils
{
	/** Gets all message action names */
	TSet<FName> GetAllMessageActionNames();
	TSet<EConcertLogMessageAction> GetAllMessageActions();

	FName ConvertActionToName(EConcertLogMessageAction MessageAction);
	FString GetActionDisplayString(EConcertLogMessageAction MessageAction);
	FString GetActionDisplayString(FName MessageActionName);
};
