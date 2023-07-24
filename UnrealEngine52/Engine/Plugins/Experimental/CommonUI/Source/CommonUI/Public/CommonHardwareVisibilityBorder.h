// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonBorder.h"
#include "GameplayTagContainer.h"
#include "CommonHardwareVisibilityBorder.generated.h"

enum class ESlateVisibility : uint8;

class UCommonUIVisibilitySubsystem;

/**
 * A container that controls visibility based on Platform, Input 
 */
UCLASS()
class COMMONUI_API UCommonHardwareVisibilityBorder : public UCommonBorder
{
	GENERATED_UCLASS_BODY()

public:
	

protected:

	UPROPERTY(EditAnywhere, Category = "Visibility", meta=(Categories="Input,Platform.Trait"))
	FGameplayTagQuery VisibilityQuery;

	UPROPERTY(EditAnywhere, Category = "Visibility")
	ESlateVisibility VisibleType;
	
	UPROPERTY(EditAnywhere, Category = "Visibility")
	ESlateVisibility HiddenType;

protected:
	// Begin UWidget
	virtual void OnWidgetRebuilt() override;
	// End UWidget

	void UpdateVisibility(UCommonUIVisibilitySubsystem* VisSystem = nullptr);

	void ListenToInputMethodChanged();

	void HandleInputMethodChanged(UCommonUIVisibilitySubsystem*);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CommonUITypes.h"
#include "CoreMinimal.h"
#endif
