// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PanelWidget.h"
#include "Types/SlateEnums.h"
#include "StackBox.generated.h"

class UStackBoxSlot;
class SStackBox;

/**
 * A stack box widget is a layout panel allowing child widgets to be automatically laid out
 * vertically or horizontally.
 *
 * * Many Children
 * * Flows Vertical or Horizontal
 */
UCLASS(meta = (ShortTooltip = "A layout panel for automatically laying child widgets out vertically or horizontally"))
class UMG_API UStackBox : public UPanelWidget
{
	GENERATED_BODY()

	UStackBox();

private:
	/** The orientation of the stack box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<EOrientation> Orientation;

public:
	/** Get the orientation of the stack box. */
	EOrientation GetOrientation() const;
	/** Set the orientation of the stack box. The existing elements will be rearranged. */
	void SetOrientation(EOrientation InType);

	/** Adds a new child widget to the container. */
	UFUNCTION(BlueprintCallable, Category="Panel")
	UStackBoxSlot* AddChildToStackBox(UWidget* Content);

#if WITH_EDITOR
	//~ UWidget interface
	virtual const FText GetPaletteCategory() override;
	//~ End UWidget interface
#endif

protected:
	//~ UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	//~ End UPanelWidget

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SStackBox> MyBox;
};
