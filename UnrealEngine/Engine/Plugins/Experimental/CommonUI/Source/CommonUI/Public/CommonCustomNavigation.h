// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUITypes.h"
#include "Components/Border.h"
#include "CommonCustomNavigation.generated.h"

/**
 * Exposes a bindable event that can be used to stomp default border navigation with custom behaviors.
 */
UCLASS(Config = CommonUI, DefaultConfig, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Custom Navigation"))
class COMMONUI_API UCommonCustomNavigation : public UBorder
{
	GENERATED_UCLASS_BODY()

public:
	DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(bool, FOnCustomNavigationEvent, EUINavigation, NavigationType);

	/** Return true if the Navigation has been handled */
	UPROPERTY(EditAnywhere, Category = Events, meta = (IsBindableEvent = "True"))
	FOnCustomNavigationEvent OnNavigationEvent;

public:

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

protected:
	
	bool OnNavigation(EUINavigation NavigationType);

};