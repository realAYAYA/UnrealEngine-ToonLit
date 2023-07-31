// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ScriptMacros.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/PanelSlot.h"
#include "Layout/Margin.h"
#include "BackgroundBlurSlot.generated.h"


class SBackgroundBlur;
class UBackgroundBlur;

/**
 * The Slot for the UBackgroundBlurSlot, contains the widget displayed in a BackgroundBlur's single slot
 */
UCLASS()
class UMG_API UBackgroundBlurSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	FMargin GetPadding() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Background Blur Slot")
	void SetPadding(FMargin InPadding);

	EHorizontalAlignment GetHorizontalAlignment() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Background Blur Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	EVerticalAlignment GetVerticalAlignment() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Background Blur Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

protected:
	/** The padding area between the slot and the content it contains. */
	UE_DEPRECATED(5.1, "Direct access to Padding is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetPadding", Category="Layout|Background Blur Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UE_DEPRECATED(5.1, "Direct access to HorizontalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetHorizontalAlignment", Category="Layout|Background Blur Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UE_DEPRECATED(5.1, "Direct access to VerticalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetVerticalAlignment", Category="Layout|Background Blur Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	//~ Begin UPanelSlot interface
	virtual void SynchronizeProperties() override;
	//~ End of UPanelSlot interface

	/** Builds the underlying slot for the slate BackgroundBlur. */
	void BuildSlot(TSharedRef<SBackgroundBlur> InBackgroundBlur);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

public:

#if WITH_EDITOR

	//~ Begin UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End of UObject interface

#endif

private:

	/** A pointer to the BackgroundBlur to allow us to adjust the size, padding...etc at runtime. */
	TSharedPtr<SBackgroundBlur> BackgroundBlur;

	friend UBackgroundBlur;
};
