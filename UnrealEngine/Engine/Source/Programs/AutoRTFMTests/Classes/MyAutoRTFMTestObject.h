// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MyAutoRTFMTestObject.generated.h"

UCLASS()
class UMyAutoRTFMTestObject : public UObject
{
	GENERATED_BODY()

public:
	UMyAutoRTFMTestObject() : Value(42)
	{}

	int Value;
};
