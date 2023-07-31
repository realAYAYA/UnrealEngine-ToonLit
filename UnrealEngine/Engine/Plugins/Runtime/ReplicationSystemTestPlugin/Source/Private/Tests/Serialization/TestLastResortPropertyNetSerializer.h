// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/ObjectMacros.h"
#include "Internationalization/Text.h"
#include "TestLastResortPropertyNetSerializer.generated.h"

USTRUCT()
struct FStructForLastResortPropertyNetSerializerTest
{
	GENERATED_BODY()

	UPROPERTY()
	FText Text;
};
