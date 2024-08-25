// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "AudioPropertiesBindings.generated.h"

UCLASS()
class AUDIOEXTENSIONS_API UAudioPropertiesBindings : public UObject
{
	GENERATED_BODY()

	UAudioPropertiesBindings() = default;

public:
	UPROPERTY(EditDefaultsOnly, Category = "Default")
	TMap<FName, FName> ObjectPropertyToSheetPropertyMap;
};