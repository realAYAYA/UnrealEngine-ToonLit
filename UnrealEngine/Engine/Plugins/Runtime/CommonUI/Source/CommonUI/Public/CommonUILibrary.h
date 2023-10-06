// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CommonUILibrary.generated.h"

class UWidget;
template <typename T> class TSubclassOf;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#endif
