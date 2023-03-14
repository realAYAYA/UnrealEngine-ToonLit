// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PanelWidget.h"
#include "SWidgetCarousel.h"

class UCommonWidgetCarousel;

#include "CommonWidgetCarousel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCurrentPageIndexChanged, UCommonWidgetCarousel*, CarouselWidget, int32, CurrentPageIndex);

/**
 * A widget switcher is like a tab control, but without tabs. At most one widget is visible at time.
 */
UCLASS(Blueprintable)
class COMMONUI_API UCommonWidgetCarousel : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The slot index to display */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carousel", meta=( UIMin=0, ClampMin=0 ))
	int32 ActiveWidgetIndex;

public:

	/** Gets the slot index of the currently active widget */
	UFUNCTION(BlueprintCallable, Category="Carousel")
	int32 GetActiveWidgetIndex() const;

	/** Activates the widget at the specified index. */
	UFUNCTION(BlueprintCallable, Category="Carousel")
	virtual void SetActiveWidgetIndex( int32 Index );

	/** Activates the widget and makes it the active index. */
	UFUNCTION(BlueprintCallable, Category="Carousel")
	virtual void SetActiveWidget(UWidget* Widget);

	/** Get a widget at the provided index */
	UFUNCTION( BlueprintCallable, Category = "Carousel" )
	UWidget* GetWidgetAtIndex( int32 Index ) const;

	UFUNCTION(BlueprintCallable, Category="Carousel")
	void BeginAutoScrolling(float ScrollInterval = 10);

	UFUNCTION(BlueprintCallable, Category="Carousel")
	void EndAutoScrolling();

	UFUNCTION(BlueprintCallable, Category="Carousel")
	void NextPage();

	UFUNCTION(BlueprintCallable, Category="Carousel")
	void PreviousPage();
	
	UPROPERTY(BlueprintAssignable, Category = "Carousel")
	FOnCurrentPageIndexChanged OnCurrentPageIndexChanged;

	// UWidget interface
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UWidget interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual void OnDescendantSelectedByDesigner(UWidget* DescendantWidget) override;
	virtual void OnDescendantDeselectedByDesigner(UWidget* DescendantWidget) override;

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
#endif

protected:
	bool AutoScrollCallback(float DeltaTime);

	TSharedRef<SWidget> OnGenerateWidgetForCarousel(UPanelSlot* PanelSlot);
	void HandlePageChanged(int32 PageIndex);

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	// End UPanelWidget

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:
	FTSTicker::FDelegateHandle TickerHandle;

	TSharedPtr< SWidgetCarousel<UPanelSlot*> > MyCommonWidgetCarousel;

	TArray< TSharedRef<SWidget> > CachedSlotWidgets;
};
