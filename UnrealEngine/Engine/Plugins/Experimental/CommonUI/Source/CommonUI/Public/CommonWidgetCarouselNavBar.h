// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PanelWidget.h"

class UCommonWidgetCarousel;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class SHorizontalBox;

#include "CommonWidgetCarouselNavBar.generated.h"

/**
 * A Navigation control for a Carousel
 */
UCLASS(Blueprintable)
class COMMONUI_API UCommonWidgetCarouselNavBar : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CarouselNavBar")
	TSubclassOf<UCommonButtonBase> ButtonWidgetType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CarouselNavBar")
	FMargin ButtonPadding;

	/**
	 * Establishes the Widget Carousel instance that this Nav Bar should interact with
	 * @param CommonCarousel The carousel that this nav bar should be associated with and manipulate
	 */
	UFUNCTION(BlueprintCallable, Category = "CarouselNavBar")
	void SetLinkedCarousel(UCommonWidgetCarousel* CommonCarousel);


	// UWidget interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UWidget interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	UFUNCTION()
	void HandlePageChanged(UCommonWidgetCarousel* CommonCarousel, int32 PageIndex);

	UFUNCTION()
	void HandleButtonClicked(UCommonButtonBase* AssociatedButton, int32 ButtonIndex);

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	void RebuildButtons();

protected:
	TSharedPtr<SHorizontalBox> MyContainer;
	
	UPROPERTY()
	TObjectPtr<UCommonWidgetCarousel> LinkedCarousel;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> ButtonGroup;

	UPROPERTY()
	TArray<TObjectPtr<UCommonButtonBase>> Buttons;
};
