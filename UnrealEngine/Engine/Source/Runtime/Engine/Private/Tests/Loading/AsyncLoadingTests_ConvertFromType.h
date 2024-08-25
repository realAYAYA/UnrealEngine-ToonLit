// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/ObjectMacros.h"
#include "AsyncLoadingTests_ConvertFromType.generated.h"

UCLASS()
class UAsyncLoadingTests_ConvertFromType_V1 : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSoftObjectPtr<UObject> Reference;
};

UCLASS()
class UAsyncLoadingTests_ConvertFromType_V2 : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UObject> Reference;
};
