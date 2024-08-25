// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "PropertyColorSettings.generated.h"

USTRUCT()
struct FPropertyColorCustomProperty
{
	GENERATED_BODY()

	UPROPERTY(Config)
	FName Name;

	UPROPERTY(Config)
	FString Text;

	UPROPERTY(Config)
	FString PropertyChain;

	UPROPERTY(Config)
	FString PropertyValue;

	UPROPERTY(Config)
	FColor PropertyColor = FColor::Red;

	UPROPERTY(Config)
	FColor DefaultColor = FColor::White;
};

/**
* Implements the settings for Property Color.
*/
UCLASS(config = EditorSettings)
class UNREALED_API UPropertyColorSettings : public UObject
{
	GENERATED_BODY()

public:
	UPropertyColorSettings()
	{}

	UPROPERTY(Config)
	TArray<FPropertyColorCustomProperty> CustomProperties;
};