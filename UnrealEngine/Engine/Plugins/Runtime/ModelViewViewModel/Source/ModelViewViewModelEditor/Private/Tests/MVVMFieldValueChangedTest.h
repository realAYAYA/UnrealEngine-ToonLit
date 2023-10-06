// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMViewModelBase.h"


#include "MVVMFieldValueChangedTest.generated.h"

UCLASS(MinimalAPI, Transient, hidedropdown, Hidden)
class UMVVMFieldValueChangedTest : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "MVVM")
	int32 PropertyInt;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MVVM")
	float PropertyFloat;

	UFUNCTION(BlueprintCallable, FieldNotify, Category = "MVVM")
	int32 FunctionInt() const
	{
		return 0;
	}

	UFUNCTION(BlueprintCallable, FieldNotify, Category = "MVVM")
	float FunctionFloat() const
	{
		return 1.0f;
	}
};
