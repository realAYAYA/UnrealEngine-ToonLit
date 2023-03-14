// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Widgets/SOverlay.h"
#include "Components/PanelSlot.h"


#include "OverlaySlot.generated.h"

/** Slot for the UOverlay panel.  Allows content to be hover above other content. */
UCLASS()
class UMG_API UOverlaySlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

protected:
	//TODO UMG Slots should hold weak or shared refs to slots.

	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SOverlay::FOverlaySlot* Slot;

public:
	
	/** The padding area between the slot and the content it contains. */
	UE_DEPRECATED(5.1, "Direct access to Padding is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = SetPadding, Category="Layout|Overlay Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UE_DEPRECATED(5.1, "Direct access to HorizontalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = SetHorizontalAlignment, Category="Layout|Overlay Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UE_DEPRECATED(5.1, "Direct access to VerticalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = SetVerticalAlignment, Category="Layout|Overlay Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	/** Set padding area between the slot and the content it contains. */
	UFUNCTION(BlueprintCallable, Category="Layout|Overlay Slot")
	void SetPadding(FMargin InPadding);

	/** Get padding area between the slot and the content it contains. */
	FMargin GetPadding() const;

	/** Set the alignment of the object horizontally. */
	UFUNCTION(BlueprintCallable, Category="Layout|Overlay Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);
	
	/** Get the alignment of the object horizontally. */
	EHorizontalAlignment GetHorizontalAlignment() const;

	/** Set the alignment of the object vertically. */
	UFUNCTION(BlueprintCallable, Category="Layout|Overlay Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	/** Get the alignment of the object vertically. */
	EVerticalAlignment GetVerticalAlignment() const;

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying FSlot for the Slate layout panel. */
	virtual void BuildSlot(TSharedRef<SOverlay> InOverlay);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
};
