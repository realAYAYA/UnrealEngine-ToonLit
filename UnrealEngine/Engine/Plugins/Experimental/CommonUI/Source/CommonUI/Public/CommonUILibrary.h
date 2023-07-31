// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/Widget.h"
#include "CommonUILibrary.generated.h"

UCLASS()
class COMMONUI_API UCommonUILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Finds the first parent widget of the given type and returns it, or null if no parent could be found.
	 */
	UFUNCTION(BlueprintCallable, Category="Common UI", meta=(DeterminesOutputType=Type))
	static UWidget* FindParentWidgetOfType(UWidget* StartingWidget, TSubclassOf<UWidget> Type);
};
