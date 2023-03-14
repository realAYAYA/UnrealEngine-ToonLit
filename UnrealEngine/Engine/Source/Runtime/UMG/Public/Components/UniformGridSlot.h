// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Components/PanelSlot.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#include "UniformGridSlot.generated.h"

/**
 * A slot for UUniformGridPanel, these slots all share the same size as the largest slot
 * in the grid.
 */
UCLASS()
class UMG_API UUniformGridSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	/** The alignment of the object horizontally. */
	UE_DEPRECATED(5.1, "Direct access to HorizontalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetHorizontalAlignment", Category="Layout|Uniform Grid Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UE_DEPRECATED(5.1, "Direct access to VerticalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetVerticalAlignment", Category="Layout|Uniform Grid Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;
	
	/** The row index of the cell this slot is in */
	UE_DEPRECATED(5.1, "Direct access to Row is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetRow", meta=( UIMin = "0" ), Category="Layout|Uniform Grid Slot")
	int32 Row;
	
	/** The column index of the cell this slot is in */
	UE_DEPRECATED(5.1, "Direct access to Column is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, BlueprintSetter="SetColumn", meta=( UIMin = "0" ), Category="Layout|Uniform Grid Slot")
	int32 Column;

public:

	/** Gets the row index of the slot */
	int32 GetRow() const;

	/** Sets the row index of the slot, this determines what cell the slot is in the panel */
	UFUNCTION(BlueprintCallable, Category="Layout|Uniform Grid Slot")
	void SetRow(int32 InRow);

	/** Gets the column index of the slot. */
	int32 GetColumn() const;

	/** Sets the column index of the slot, this determines what cell the slot is in the panel */
	UFUNCTION(BlueprintCallable, Category="Layout|Uniform Grid Slot")
	void SetColumn(int32 InColumn);

	/**  */
	EHorizontalAlignment GetHorizontalAlignment() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Layout|Uniform Grid Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	/**  */
	EVerticalAlignment GetVerticalAlignment() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Layout|Uniform Grid Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

public:

	//~ UPanelSlot interface
	virtual void SynchronizeProperties() override;
	//~ End of UPanelSlot interface

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SUniformGridPanel> GridPanel);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual bool NudgeByDesigner(const FVector2D& NudgeDirection, const TOptional<int32>& GridSnapSize) override;
	virtual void SynchronizeFromTemplate(const UPanelSlot* const TemplateSlot) override;
#endif //WITH_EDITOR

private:
	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SUniformGridPanel::FSlot* Slot;
};
