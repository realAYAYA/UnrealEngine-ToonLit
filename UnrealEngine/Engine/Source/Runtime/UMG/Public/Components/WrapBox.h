// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Components/PanelWidget.h"
#include "WrapBox.generated.h"

class UWrapBoxSlot;

/**
 * Arranges widgets left-to-right or top-to-bottom dependently of the orientation.  When the widgets exceed the wrapSize it will place widgets on the next line.
 * 
 * * Many Children
 * * Flows
 * * Wraps
 */
UCLASS()
class UMG_API UWrapBox : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The inner slot padding goes between slots sharing borders */
	UE_DEPRECATED(5.1, "Direct access to InnerSlotPadding is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetInnerSlotPadding", Category = "Content Layout")
	FVector2D InnerSlotPadding;

	/** When this size is exceeded, elements will start appearing on the next line. */
	UE_DEPRECATED(5.1, "Direct access to WrapSize is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Content Layout", meta=(EditCondition = "bExplicitWrapSize"))
	float WrapSize;

	/** Use explicit wrap size whenever possible. It greatly simplifies layout calculations and reduces likelihood of "wiggling UI" */
	UE_DEPRECATED(5.1, "Direct access to bExplicitWrapSize is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter="UseExplicitWrapSize", Setter = "SetExplicitWrapSize", Category = "Content Layout")
	bool bExplicitWrapSize;

	/** The alignment of each line of wrapped content. */
	UE_DEPRECATED(5.1, "Direct access to HorizontalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetHorizontalAlignment", Category = "Content Layout")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** Determines if the Wrap Box should arranges the widgets left-to-right or top-to-bottom */
	UE_DEPRECATED(5.1, "Direct access to Orientation is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Content Layout")
	TEnumAsByte<EOrientation> Orientation = EOrientation::Orient_Horizontal;

public:
	/** Gets the inner slot padding goes between slots sharing borders */
	FVector2D GetInnerSlotPadding() const;

	/** Sets the inner slot padding goes between slots sharing borders */
	UFUNCTION(BlueprintCallable, Category="Content Layout")
	void SetInnerSlotPadding(FVector2D InPadding);

	/** */
	float GetWrapSize() const;

	/** */
	void SetWrapSize(float InWrapSize);

	/** */
	bool UseExplicitWrapSize() const;

	/** */
	void SetExplicitWrapSize(bool bInExplicitWrapSize);

	/** */
	EHorizontalAlignment GetHorizontalAlignment() const;

	/** */
	UFUNCTION(BlueprintCallable, Category="Content Layout")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	/** */
	EOrientation GetOrientation() const;

	/** */
	void SetOrientation(EOrientation InOrientation);

public:
	UFUNCTION(BlueprintCallable, Category="Panel")
	UWrapBoxSlot* AddChildToWrapBox(UWidget* Content);

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
	//~ End UPanelWidget

	//~ UWidget interface
	virtual void SynchronizeProperties() override;
	//~ End of UWidget interface

	//~ UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End of UVisual interface

protected:

	TSharedPtr<class SWrapBox> MyWrapBox;

protected:
	//~ UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End of UWidget interface
};
