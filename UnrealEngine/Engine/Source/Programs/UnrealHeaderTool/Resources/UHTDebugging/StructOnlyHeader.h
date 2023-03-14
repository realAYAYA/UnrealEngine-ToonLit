// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructOnlyHeader.generated.h"

DECLARE_DYNAMIC_DELEGATE(FSimpleStructDelegate);

USTRUCT()
struct FSomeStruct
{
#pragma region X

	GENERATED_BODY()

#pragma endregion X
};

USTRUCT()
struct alignas(8) FAlignedStruct
{
	GENERATED_BODY()

	UPROPERTY()
	FSimpleStructDelegate DelegateProp;
};
