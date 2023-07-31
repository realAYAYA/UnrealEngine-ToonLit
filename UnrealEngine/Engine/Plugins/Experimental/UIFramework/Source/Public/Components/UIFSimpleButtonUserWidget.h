// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "UIFSimpleButtonUserWidget.generated.h"

class UButton;
class UTextBlock;

/**
 *
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class UUIFrameworkSimpleButtonUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "UI Framework", meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "UI Framework", meta = (BindWidget))
	TObjectPtr<UButton> Button = nullptr;
};
