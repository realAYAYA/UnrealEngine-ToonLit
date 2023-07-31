// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformInput.h"

class FString;

struct INPUTCORE_API FLinuxPlatformInput : FGenericPlatformInput
{
	static uint32 GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings );
	static uint32 GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings);
};

typedef FLinuxPlatformInput FPlatformInput;
