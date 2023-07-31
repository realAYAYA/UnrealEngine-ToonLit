// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "MassActorPoolableInterface.generated.h"

UINTERFACE(Blueprintable)
class MASSACTORS_API UMassActorPoolableInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class MASSACTORS_API IMassActorPoolableInterface : public IInterface
{
	GENERATED_IINTERFACE_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mass|Actor Pooling")
	bool CanBePooled();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mass|Actor Pooling")
	void PrepareForPooling();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mass|Actor Pooling")
	void PrepareForGame();
};