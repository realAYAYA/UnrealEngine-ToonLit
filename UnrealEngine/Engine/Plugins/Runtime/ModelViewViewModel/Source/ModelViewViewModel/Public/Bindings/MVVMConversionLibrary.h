// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MVVMConversionLibrary.generated.h"

UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMConversionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	/**
	 * Converts a bool to a slate visibility.
	 */
	UFUNCTION(BlueprintPure, Category = "Widget", meta = (DisplayName = "To Visibility (Boolean)"))
	static ESlateVisibility Conv_BoolToSlateVisibility(bool bIsVisible, ESlateVisibility TrueVisibility = ESlateVisibility::Visible, ESlateVisibility FalseVisibility = ESlateVisibility::Collapsed);
};