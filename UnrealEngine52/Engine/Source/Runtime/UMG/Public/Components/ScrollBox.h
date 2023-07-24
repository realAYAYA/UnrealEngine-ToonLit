// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Components/PanelWidget.h"
#include "Containers/Ticker.h"
#include "ScrollBox.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserScrolledEvent, float, CurrentOffset);

/**
 * An arbitrary scrollable collection of widgets.  Great for presenting 10-100 widgets in a list.  Doesn't support virtualization.
 */
UCLASS()
class UMG_API UScrollBox : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:

	UE_DEPRECATED(5.2, "Direct access to WidgetStyle is deprecated. Please use the getter or setter.")
	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Style", meta=( DisplayName="Style" ))
	FScrollBoxStyle WidgetStyle;

	UE_DEPRECATED(5.2, "Direct access to WidgetBarStyle is deprecated. Please use the getter or setter.")
	/** The bar style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Style", meta=( DisplayName="Bar Style" ))
	FScrollBarStyle WidgetBarStyle;

	UE_DEPRECATED(5.2, "Direct access to Orientation is deprecated. Please use the getter or setter.")
	/** The orientation of the scrolling and stacking in the box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetOrientation", Category="Scroll")
	TEnumAsByte<EOrientation> Orientation;

	UE_DEPRECATED(5.2, "Direct access to ScrollBarVisibility is deprecated. Please use the getter or setter.")
	/** Visibility */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetScrollBarVisibility", Category="Scroll")
	ESlateVisibility ScrollBarVisibility;

	UE_DEPRECATED(5.2, "Direct access to ConsumeMouseWheel is deprecated. Please use the getter or setter.")
	/** When mouse wheel events should be consumed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetConsumeMouseWheel", Category = "Scroll")
	EConsumeMouseWheel ConsumeMouseWheel;

	UE_DEPRECATED(5.2, "Direct access to ScrollbarThickness is deprecated. Please use the getter or setter.")
	/** The thickness of the scrollbar thumb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetScrollbarThickness", Category="Scroll")
	FVector2D ScrollbarThickness;

	UE_DEPRECATED(5.2, "Direct access to ScrollbarPadding is deprecated. Please use the getter or setter.")
	/** The margin around the scrollbar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetScrollbarPadding", Category = "Scroll")
	FMargin ScrollbarPadding;

	/**  */
	UE_DEPRECATED(5.2, "Direct access to AlwaysShowScrollbar is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsAlwaysShowScrollbar", Setter, BlueprintSetter = "SetAlwaysShowScrollbar", Category = "Scroll")
	bool AlwaysShowScrollbar;

	/**  */
	UE_DEPRECATED(5.2, "Direct access to AlwaysShowScrollbarTrack is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsAlwaysShowScrollbarTrack", Setter, Category = "Scroll")
	bool AlwaysShowScrollbarTrack;

	UE_DEPRECATED(5.2, "Direct access to AllowOverscroll is deprecated. Please use the getter or setter.")
	/**  Disable to stop scrollbars from activating inertial overscrolling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsAllowOverscroll", Setter, BlueprintSetter = "SetAllowOverscroll", Category = "Scroll")
	bool AllowOverscroll;

	UE_DEPRECATED(5.2, "Direct access to BackPadScrolling is deprecated. Please use the getter. Note that BackPadScrolling is only set at construction and is not modifiable at runtime.")
	/** Whether to back pad this scroll box, allowing user to scroll backward until child contents are no longer visible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Getter = "IsBackPadScrolling", Category = "Scroll")
	bool BackPadScrolling;

	UE_DEPRECATED(5.2, "Direct access to FrontPadScrolling is deprecated. Please use the getter. Note that FrontPadScrolling is only set at construction and is not modifiable at runtime.")
	/** Whether to front pad this scroll box, allowing user to scroll forward until child contents are no longer visible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Getter = "IsFrontPadScrolling", Category = "Scroll")
	bool FrontPadScrolling;
	
	UE_DEPRECATED(5.2, "Direct access to bAnimateWheelScrolling is deprecated. Please use the getter or setter.")
	/** True to lerp smoothly when wheel scrolling along the scroll box */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsAnimateWheelScrolling", Setter = "SetAnimateWheelScrolling", BlueprintSetter = "SetAnimateWheelScrolling", Category = "Scroll")
	bool bAnimateWheelScrolling = false;

	UE_DEPRECATED(5.2, "Direct access to NavigationDestination is deprecated. Please use the getter or setter.")
	/** Sets where to scroll a widget to when using explicit navigation or if ScrollWhenFocusChanges is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetNavigationDestination", Getter, Setter, Category = "Scroll")
	EDescendantScrollDestination NavigationDestination;

	UE_DEPRECATED(5.2, "Direct access to NavigationScrollPadding is deprecated. Please use the getter. Note that NavigationScrollPadding is only set at construction and is not modifiable at runtime.")
	/**
	 * The amount of padding to ensure exists between the item being navigated to, at the edge of the
	 * scrollbox.  Use this if you want to ensure there's a preview of the next item the user could scroll to.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Getter, Category="Scroll")
	float NavigationScrollPadding;

	UE_DEPRECATED(5.2, "Direct access to ScrollWhenFocusChanges is deprecated. Please use the getter or setter.")
	/** Scroll behavior when user focus is given to a child widget */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetScrollWhenFocusChanges", Category="Scroll", meta=(DisplayName="Scroll When Focus Changes"))
	EScrollWhenFocusChanges ScrollWhenFocusChanges;
	
	UE_DEPRECATED(5.2, "Direct access to bAllowRightClickDragScrolling is deprecated. Please use the getter or setter.")
	/** Option to disable right-click-drag scrolling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsAllowRightClickDragScrolling", Setter = "SetAllowRightClickDragScrolling", Category = "Scroll")
	bool bAllowRightClickDragScrolling;

	UE_DEPRECATED(5.2, "Direct access to WheelScrollMultiplier is deprecated. Please use the getter or setter.")
	/** The multiplier to apply when wheel scrolling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetWheelScrollMultiplier", Category = "Scroll")
	float WheelScrollMultiplier = 1.f;

	void SetWidgetStyle(const FScrollBoxStyle& NewWidgetStyle);

	const FScrollBoxStyle& GetWidgetStyle() const;

	void SetWidgetBarStyle(const FScrollBarStyle& NewWidgetBarStyle);

	const FScrollBarStyle& GetWidgetBarStyle() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetConsumeMouseWheel(EConsumeMouseWheel NewConsumeMouseWheel);

	EConsumeMouseWheel GetConsumeMouseWheel() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetOrientation(EOrientation NewOrientation);

	EOrientation GetOrientation() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetScrollBarVisibility(ESlateVisibility NewScrollBarVisibility);

	ESlateVisibility GetScrollBarVisibility() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetScrollbarThickness(const FVector2D& NewScrollbarThickness);

	FVector2D GetScrollbarThickness() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetScrollbarPadding(const FMargin& NewScrollbarPadding);

	FMargin GetScrollbarPadding() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetAlwaysShowScrollbar(bool NewAlwaysShowScrollbar);
	
	bool IsAlwaysShowScrollbar() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetAllowOverscroll(bool NewAllowOverscroll);

	bool IsAllowOverscroll() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetAnimateWheelScrolling(bool bShouldAnimateWheelScrolling);

	bool IsAnimateWheelScrolling() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetWheelScrollMultiplier(float NewWheelScrollMultiplier);

	float GetWheelScrollMultiplier() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetScrollWhenFocusChanges(EScrollWhenFocusChanges NewScrollWhenFocusChanges);

	EScrollWhenFocusChanges GetScrollWhenFocusChanges() const;

	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void SetNavigationDestination(const EDescendantScrollDestination NewNavigationDestination);

	EDescendantScrollDestination GetNavigationDestination() const;

	void SetAlwaysShowScrollbarTrack(bool NewAlwaysShowScrollbarTrack);

	bool IsAlwaysShowScrollbarTrack() const;

	float GetNavigationScrollPadding() const;

	void SetAllowRightClickDragScrolling(bool bShouldAllowRightClickDragScrolling);

	bool IsAllowRightClickDragScrolling() const;

	bool IsFrontPadScrolling() const;

	bool IsBackPadScrolling() const;

	/** Instantly stops any inertial scrolling that is currently in progress */
	UFUNCTION(BlueprintCallable, Category = "Scroll")
	void EndInertialScrolling();

public:

	/** Called when the scroll has changed */
	UPROPERTY(BlueprintAssignable, Category = "Button|Event")
	FOnUserScrolledEvent OnUserScrolled;

	/**
	 * Updates the scroll offset of the scrollbox.
	 * @param NewScrollOffset is in Slate Units.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetScrollOffset(float NewScrollOffset);
	
	/**
	 * Gets the scroll offset of the scrollbox in Slate Units.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	float GetScrollOffset() const;

	/** Gets the scroll offset of the bottom of the ScrollBox in Slate Units. */
	UFUNCTION(BlueprintCallable, Category = "Widget")
	float GetScrollOffsetOfEnd() const;

	/** Gets the fraction currently visible in the scrollbox */
	UFUNCTION(BlueprintCallable, Category="Widget")
	float GetViewFraction() const;

	UFUNCTION(BlueprintCallable, Category="Widget")
	float GetViewOffsetFraction() const;

	/** Scrolls the ScrollBox to the top instantly */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void ScrollToStart();

	/** Scrolls the ScrollBox to the bottom instantly during the next layout pass. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void ScrollToEnd();

	/** Scrolls the ScrollBox to the widget during the next layout pass. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void ScrollWidgetIntoView(UWidget* WidgetToFind, bool AnimateScroll = true, EDescendantScrollDestination ScrollDestination = EDescendantScrollDestination::IntoView, float Padding = 0);

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
#if WITH_EDITORONLY_DATA
	virtual void Serialize(FArchive& Ar) override;
#endif // if WITH_EDITORONLY_DATA
	//~ End UObject Interface

#if WITH_EDITOR
	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	virtual void OnDescendantSelectedByDesigner( UWidget* DescendantWidget ) override;
	virtual void OnDescendantDeselectedByDesigner( UWidget* DescendantWidget ) override;
	//~ End UWidget Interface
#endif

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

	void SlateHandleUserScrolled(float CurrentOffset);

	// Initialize IsFocusable in the constructor before the SWidget is constructed.
	void InitBackPadScrolling(bool InBackPadScrolling);
	// Initialize IsFocusable in the constructor before the SWidget is constructed.
	void InitFrontPadScrolling(bool InFrontPadScrolling);
	// Initialize IsFocusable in the constructor before the SWidget is constructed.
	void InitNavigationScrollPadding(float InNavigationScrollPadding);

protected:
	/** The desired scroll offset for the underlying scrollbox.  This is a cache so that it can be set before the widget is constructed. */
	float DesiredScrollOffset;

	TSharedPtr<class SScrollBox> MyScrollBox;

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

#if WITH_EDITOR
	FTSTicker::FDelegateHandle TickHandle;
#endif //WITH_EDITOR
};
