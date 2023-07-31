// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/UnrealPakCommandlet.h"
#include "PakFileUtilities.h"

UUnrealPakCommandlet::UUnrealPakCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UUnrealPakCommandlet::Main(const FString& Params)
{
	return ExecuteUnrealPak(*Params)? 0 : 1;
}
