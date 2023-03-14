// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TestInterface.generated.h"

UINTERFACE()
class UTestInterface : public UInterface
{
	GENERATED_BODY()
public:
	UTestInterface(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

class ITestInterface
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintNativeEvent)
	FString SomeFunction(int32 Val) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	const float SomeFunctionWithConstReturnType() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	UObject* SomeFunctionWithNonConstPointerReturnType() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	const UObject* SomeFunctionWithConstPointerReturnType() const;

	UFUNCTION(BlueprintImplementableEvent)
	int64 ReturnInt64();
};
