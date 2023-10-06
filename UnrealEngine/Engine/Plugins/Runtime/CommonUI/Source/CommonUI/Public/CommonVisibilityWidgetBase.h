// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonBorder.h"
#include "CommonVisibilityWidgetBase.generated.h"

enum class ECommonInputType : uint8;
enum class ESlateVisibility : uint8;

/**
 * A container that controls visibility based on Input type and Platform
 *
 */
UCLASS(Deprecated)
class COMMONUI_API UDEPRECATED_UCommonVisibilityWidgetBase : public UCommonBorder
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Visibility", meta = (GetOptions = GetRegisteredPlatforms))
	TMap<FName, bool> VisibilityControls;

	UPROPERTY(EditAnywhere, Category = "Visibility")
	bool bShowForGamepad;

	UPROPERTY(EditAnywhere, Category = "Visibility")
	bool bShowForMouseAndKeyboard;

	UPROPERTY(EditAnywhere, Category = "Visibility")
	bool bShowForTouch;

	UPROPERTY(EditAnywhere, Category = "Visibility")
	ESlateVisibility VisibleType;
	
	UPROPERTY(EditAnywhere, Category = "Visibility")
	ESlateVisibility HiddenType;

protected:
	// Begin UWidget
	virtual void OnWidgetRebuilt() override;
	// End UWidget

	void UpdateVisibility();

	void ListenToInputMethodChanged(bool bListen = true);

	void HandleInputMethodChanged(ECommonInputType input);

	UFUNCTION()
	static const TArray<FName>& GetRegisteredPlatforms();
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CommonUITypes.h"
#include "CoreMinimal.h"
#endif
