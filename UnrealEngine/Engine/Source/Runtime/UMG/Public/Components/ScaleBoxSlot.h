// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Components/PanelSlot.h"

#include "ScaleBoxSlot.generated.h"

class SScaleBox;

/** The Slot for the UScaleBoxSlot, contains the widget displayed in a button's single slot */
UCLASS()
class UMG_API UScaleBoxSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITORONLY_DATA
	/** The padding area between the slot and the content it contains. */
	UE_DEPRECATED(5.1, "Padding is not used.")
	UPROPERTY()
	FMargin Padding_DEPRECATED;
#endif

	/** The alignment of the object horizontally. */
	UE_DEPRECATED(5.1, "Direct access to HorizontalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Setter, Category="Layout|ScaleBox Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UE_DEPRECATED(5.1, "Direct access to VerticalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Setter, Category="Layout|ScaleBox Slot" )
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:
	UE_DEPRECATED(5.1, "SetPadding is deprecated")
	UFUNCTION(BlueprintCallable, Category="Layout|ScaleBox Slot", meta = (DeprecatedFunction, DeprecatedMessage = "The function is deprecated - There is no padding on ScaleBox."))
	void SetPadding(FMargin InPadding);

	EHorizontalAlignment GetHorizontalAlignment() const;

	UFUNCTION(BlueprintCallable, Category="Layout|ScaleBox Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);


	EVerticalAlignment GetVerticalAlignment() const;

	UFUNCTION(BlueprintCallable, Category="Layout|ScaleBox Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	//~ UPanelSlot interface
	virtual void SynchronizeProperties() override;
	//~ End of UPanelSlot interface

	/** Builds the underlying slot for the slate ScaleBox. */
	void BuildSlot(TSharedRef<SScaleBox> InScaleBox);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	/** A pointer to the button to allow us to adjust the size, padding...etc at runtime. */
	TWeakPtr<SScaleBox> ScaleBox;
};
