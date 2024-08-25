// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "VLogDetailsObject.generated.h"

USTRUCT()
struct FVisualLogDetails
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, Category=Details)
	FName Category;
	
	UPROPERTY(VisibleAnywhere, Category=Details)
	FString Description;
};

UCLASS()
class UVLogDetailsObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category=Details)
	TArray<FVisualLogDetails> VisualLogDetails;
};