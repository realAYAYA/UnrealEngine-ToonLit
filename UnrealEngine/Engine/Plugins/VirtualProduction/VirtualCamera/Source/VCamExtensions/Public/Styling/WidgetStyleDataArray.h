// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetStyleDataArray.generated.h"

class UWidgetStyleData;

USTRUCT()
struct VCAMEXTENSIONS_API FWidgetStyleDataArray
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Instanced, Category = "Virtual Camera")
	TArray<TObjectPtr<UWidgetStyleData>> Styles;
};