// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "InternalUniqueNetIdReplNetSerializer.generated.h"

USTRUCT()
struct FUniqueNetIdReplNetSerializerStringStruct
{
	GENERATED_BODY()

	UPROPERTY()
	FString String;
};

USTRUCT()
struct FUniqueNetIdReplNetSerializerNameStruct
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;
};
