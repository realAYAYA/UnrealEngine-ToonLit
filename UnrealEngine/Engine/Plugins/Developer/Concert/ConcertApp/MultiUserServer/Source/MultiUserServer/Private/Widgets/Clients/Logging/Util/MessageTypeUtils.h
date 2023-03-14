// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::MultiUserServer::MessageTypeUtils
{
	/** Gets all message type names (sub-structs of FConcertMessageData - see FConcertLog::MessageTypeName) */
	TSet<FName> GetAllMessageTypeNames();
	
	TSet<FName> GetAllMessageTypeNames_EventsOnly();
	TSet<FName> GetAllMessageTypeNames_RequestsOnly();
	TSet<FName> GetAllMessageTypeNames_ResponseOnlyOnly();
	TSet<FName> GetAllMessageTypeNames_AcksOnlyOnly();

	/** Gets a readable version of a message type name */
	FString SanitizeMessageTypeName(FName MessageTypeName);
};
