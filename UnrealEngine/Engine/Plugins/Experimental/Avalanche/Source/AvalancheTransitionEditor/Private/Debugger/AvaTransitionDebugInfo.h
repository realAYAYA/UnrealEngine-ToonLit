// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "StateTreeExecutionTypes.h"

/** Debug Information to identify a Tree Debug Instance */
struct FAvaTransitionDebugInfo
{
	FStateTreeInstanceDebugId Id;

	FString Name;

	FLinearColor Color;
};

#endif // WITH_STATETREE_DEBUGGER
