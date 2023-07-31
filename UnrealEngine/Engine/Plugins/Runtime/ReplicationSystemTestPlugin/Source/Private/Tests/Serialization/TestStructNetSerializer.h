// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/ObjectMacros.h"
#include "TestStructNetSerializer.generated.h"

USTRUCT()
struct FStructMemberForStructNetSerializerTest
{
	GENERATED_BODY()
};

USTRUCT()
struct FStructForStructNetSerializerTest
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Member0;

	UPROPERTY()
	uint32 Member1;

	UPROPERTY()
	uint32 Member2;
};
