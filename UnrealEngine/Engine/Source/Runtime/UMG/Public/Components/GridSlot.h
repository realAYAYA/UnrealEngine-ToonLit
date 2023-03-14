// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Components/PanelSlot.h"
#include "Widgets/Layout/SGridPanel.h"

#include "GridSlot.generated.h"

/**
 * A slot for UGridPanel, these slots all share the same size as the largest slot
 * in the grid.
 */
UCLASS()
class UMG_API UGridSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	/** The padding area between the slot and the content it contains. */
	UE_DEPRECATED(5.1, "Direct access to Padding is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetPadding", Category="Layout|Grid Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UE_DEPRECATED(5.1, "Direct access to HorizontalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetHorizontalAlignment", Category="Layout|Grid Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UE_DEPRECATED(5.1, "Direct access to VerticalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetVerticalAlignment", Category="Layout|Grid Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;
	
	/** The row index of the cell this slot is in */
	UE_DEPRECATED(5.1, "Direct access to Row is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetRow", meta=( UIMin = "0" ), Category="Layout|Grid Slot")
	int32 Row;
	
	/**  */
	UE_DEPRECATED(5.1, "Direct access to RowSpan is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetRowSpan", Category="Layout|Grid Slot")
	int32 RowSpan;

	/** The column index of the cell this slot is in */
	UE_DEPRECATED(5.1, "Direct access to Column is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetColumn", meta=( UIMin = "0" ), Category="Layout|Grid Slot")
	int32 Column;

	/**  */
	UE_DEPRECATED(5.1, "Direct access to ColumnSpan is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetColumnSpan", Category="Layout|Grid Slot")
	int32 ColumnSpan;

	/** Positive values offset this cell to be hit-tested and drawn on top of others. Default is 0; i.e. no offset. */
	UE_DEPRECATED(5.1, "Direct access to Layer is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetLayer", Category="Layout|Grid Slot")
	int32 Layer;

	/** Offset this slot's content by some amount; positive values offset to lower right */
	UE_DEPRECATED(5.1, "Direct access to Nudge is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetNudge", Category="Layout|Grid Slot")
	FVector2D Nudge;

public:

	FMargin GetPadding() const;

	UFUNCTION(BlueprintCallable, Category="Layout|Border Slot")
	void SetPadding(FMargin InPadding);

	/** Gets the row index of the slot */
	int32 GetRow() const;

	/** Sets the row index of the slot, this determines what cell the slot is in the panel */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetRow(int32 InRow);

	/** Gets how many rows this this slot spans over */
	int32 GetRowSpan() const;

	/** How many rows this this slot spans over */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetRowSpan(int32 InRowSpan);

	/** Gets the column index of the slot */
	int32 GetColumn() const;

	/** Sets the column index of the slot, this determines what cell the slot is in the panel */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetColumn(int32 InColumn);

	/** Gets how many columns this slot spans over */
	int32 GetColumnSpan() const;

	/** How many columns this slot spans over */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetColumnSpan(int32 InColumnSpan);

	/** Gets the offset this cell drawn on top of others. */
	int32 GetLayer() const;

	/** Sets positive values offset this cell to be hit-tested and drawn on top of others. */
	UFUNCTION(BlueprintCallable, Category = "Layout|Grid Slot")
	void SetLayer(int32 InLayer);

	/** Gets the offset for this slot's content */
	FVector2D GetNudge() const;

	/** Sets the offset for this slot's content by some amount; positive values offset to lower right*/
	UFUNCTION(BlueprintCallable, Category = "Layout|Grid Slot")
	void SetNudge(FVector2D InNudge);

	/** */
	EHorizontalAlignment GetHorizontalAlignment() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	/**  */
	EVerticalAlignment GetVerticalAlignment() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SGridPanel> GridPanel);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual bool NudgeByDesigner(const FVector2D& NudgeDirection, const TOptional<int32>& GridSnapSize) override;
	virtual void SynchronizeFromTemplate(const UPanelSlot* const TemplateSlot) override;
#endif //WITH_EDITOR

private:
	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SGridPanel::FSlot* Slot;
};
