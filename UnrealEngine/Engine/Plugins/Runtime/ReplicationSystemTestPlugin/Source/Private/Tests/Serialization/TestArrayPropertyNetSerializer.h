// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/ObjectMacros.h"
#include "TestArrayPropertyNetSerializer.generated.h"

USTRUCT()
struct FStructWithDynamicArrayOfPrimitiveTypeForArrayPropertyNetSerializerTest
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint32> ArrayOfUint;
};

USTRUCT()
struct FStructWithDynamicArrayOfComplexTypeForArrayPropertyNetSerializerTest
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FStructWithDynamicArrayOfPrimitiveTypeForArrayPropertyNetSerializerTest> ArrayOfStructWithArray;
};
