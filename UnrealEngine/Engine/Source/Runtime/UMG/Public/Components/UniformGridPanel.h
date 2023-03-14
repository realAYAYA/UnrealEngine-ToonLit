// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "Widgets/SWidget.h"
#include "Components/PanelWidget.h"
#include "UniformGridPanel.generated.h"

class SUniformGridPanel;
class UUniformGridSlot;

/**
 * A panel that evenly divides up available space between all of its children.
 */
UCLASS()
class UMG_API UUniformGridPanel : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** Padding given to each slot */
	UE_DEPRECATED(5.1, "Direct access to SlotPadding is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetSlotPadding", Category="Child Layout")
	FMargin SlotPadding;

	/** The minimum desired width of the slots */
	UE_DEPRECATED(5.1, "Direct access to MinDesiredSlotWidth is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetMinDesiredSlotWidth", Category="Child Layout")
	float MinDesiredSlotWidth;

	/** The minimum desired height of the slots */
	UE_DEPRECATED(5.1, "Direct access to MinDesiredSlotHeight is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetMinDesiredSlotHeight", Category="Child Layout")
	float MinDesiredSlotHeight;

public:

	/** */
	FMargin GetSlotPadding() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Child Layout")
	void SetSlotPadding(FMargin InSlotPadding);

	/** */
	float GetMinDesiredSlotWidth() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Child Layout")
	void SetMinDesiredSlotWidth(float InMinDesiredSlotWidth);

	/** */
	float GetMinDesiredSlotHeight() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Child Layout")
	void SetMinDesiredSlotHeight(float InMinDesiredSlotHeight);

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget")
	UUniformGridSlot* AddChildToUniformGrid(UWidget* Content, int32 InRow = 0, int32 InColumn = 0);

public:

	//~ UWidget interface
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
	//~ End of UWidget interface

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:

	TSharedPtr<SUniformGridPanel> MyUniformGridPanel;

protected:
	//~ UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End of UWidget interface
};
