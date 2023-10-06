// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

struct FScriptContainerElement;

/** Helper struct defining parameters we need to establish a connection */
struct FOneSkyConnectionInfo
{
	/** Name of this connection configuration */
	FString Name;

	/** OneSky ApiKey */
	FString ApiKey;

	/** OneSky ApiSecret */
	FString ApiSecret;
};
