// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Components/PanelSlot.h"

#include "WrapBoxSlot.generated.h"

/** The Slot for the UWrapBox, contains the widget that is flowed vertically */
UCLASS()
class UMG_API UWrapBoxSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	UE_DEPRECATED(5.1, "Direct access to Padding is deprecated. Please use the getter or setter.")
	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetPadding", Category="Layout|Wrap Box Slot")
	FMargin Padding;

	UE_DEPRECATED(5.1, "Direct access to FillSpanWhenLessThan is deprecated. Please use the getter or setter.")
	/**
	 * If the total available space in the wrap panel drops below this threshold, this slot will attempt to fill an entire line.
	 * NOTE: A value of 0, denotes no filling will occur.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetFillSpanWhenLessThan", Category="Layout|Wrap Box Slot")
	float FillSpanWhenLessThan;

	UE_DEPRECATED(5.1, "Direct access to HorizontalAlignment is deprecated. Please use the getter or setter.")
	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetHorizontalAlignment", Category="Layout|Wrap Box Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	UE_DEPRECATED(5.1, "Direct access to VerticalAlignment is deprecated. Please use the getter or setter.")
	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetVerticalAlignment", Category="Layout|Wrap Box Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

	UE_DEPRECATED(5.1, "Direct access to bFillEmptySpace is deprecated. Please use the getter or setter.")
	/** Should this slot fill the remaining space on the line? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetFillEmptySpace", BlueprintSetter="SetFillEmptySpace", Category = "Layout|Wrap Box Slot")
	bool bFillEmptySpace;

	UE_DEPRECATED(5.1, "Direct access to bForceNewLine is deprecated. Please use the getter or setter.")
	/** Should this slot start on a new line? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetNewLine", BlueprintSetter="SetNewLine", Category = "Layout|Wrap Box Slot")
	bool bForceNewLine;

public:

	FMargin GetPadding() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetPadding(FMargin InPadding);

	bool DoesFillEmptySpace() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetFillEmptySpace(bool InbFillEmptySpace);

	float GetFillSpanWhenLessThan() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetFillSpanWhenLessThan(float InFillSpanWhenLessThan);

	EHorizontalAlignment  GetHorizontalAlignment() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	EVerticalAlignment GetVerticalAlignment() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	bool DoesForceNewLine() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetNewLine(bool InForceNewLine);

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SWrapBox> InWrapBox);

private:
	//TODO UMG Slots should hold weak or shared refs to slots.

	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SWrapBox::FSlot* Slot;
};
