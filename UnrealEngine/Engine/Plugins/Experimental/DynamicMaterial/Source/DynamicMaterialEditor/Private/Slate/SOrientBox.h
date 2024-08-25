// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Layout/WidgetSlotWithAttributeSupport.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"
#include "Widgets/SWidget.h"

struct AlignmentArrangeResult;

/**
 * Orient Box
 * 
 * Manages vertical and horizontal layout of child slots.
 * Child slots can also have their order reversed.
 * Automatically flips HAlign, VAlign, and Padding based on orientation and reverse state.
 * 
 * Slot alignment and padding should be set for the default horizontal orientation. For example,
 * if you want bottom padding for a vertical oriented box, you should add padding to the right margin
 * and set orientation to vertical.
 */
class SOrientBox : public SPanel
{
public:
	/** A Slot that provides layout options for the contents of an oriented box. */
	using FSlot = FBasicLayoutWidgetSlot;

	static EOrientation InvertOrientation(const EOrientation InOrietation);

	static EHorizontalAlignment InvertHAlign(const EHorizontalAlignment InAlignment);
	static EVerticalAlignment InvertVAlign(const EVerticalAlignment InAlignment);

	static EVerticalAlignment HAlignToVAlign(const EHorizontalAlignment InAlignment);
	static EHorizontalAlignment VAlignToHAlign(const EVerticalAlignment InAlignment);

	static FMargin InvertPadding(const FMargin& InPadding);
	static FMargin InvertHorizontalPadding(const FMargin& InPadding);
	static FMargin InvertVerticalPadding(const FMargin& InPadding);

	static AlignmentArrangeResult AlignChildSlot(const EOrientation InOrientation, const EFlowDirection InLayoutFlow, const FVector2D InAllottedSize, const FSlot& InChildToArrange, const FMargin& InSlotPadding, const float& InContentScale = 1.0f, bool bInClampToParent = true);

	/**
	 * Creates a new widget slot.
	 *
	 * @return A new slot.
	 */
	static FSlot::FSlotArguments Slot();

	SLATE_BEGIN_ARGS(SOrientBox)
		: _Orientation(EOrientation::Orient_Horizontal)
		, _Reverse(false)
		{}
		SLATE_SLOT_ARGUMENT(FSlot, Slots)
		SLATE_ATTRIBUTE(EOrientation, Orientation)
		SLATE_ATTRIBUTE(bool, Reverse)
	SLATE_END_ARGS()

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	/**
		* Adds a slot to the widget switcher at the specified location.
		*
		* @param SlotIndex The index at which to insert the slot, or INDEX_NONE to append.
		*/
	FScopedWidgetSlotArguments AddSlot(const int32 SlotIndex = INDEX_NONE);

	/** Removes a slot at the specified location. */
	void RemoveSlot(const TSharedRef<SWidget>& WidgetToRemove);

	/** Removes all children from the box. */
	void ClearChildren();

	TPanelChildren<FSlot> Children;

	SOrientBox();

	void Construct(const FArguments& InArgs);

	//~ Begin SWidget
	virtual FChildren* GetChildren() override { return &Children; }
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual void OnArrangeChildren(const FGeometry& InAllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	//~ End SWidget

	EOrientation GetOrientation() const { return CachedOrientation; }
	void SetOrientation(const EOrientation InOrientation);

	bool IsReversed() const { return CachedReverse; }
	void SetReversed(const bool bInReversed);

	bool IsContentVertical() const;
	bool IsContentHorizontal() const;
	bool IsContentReversed() const;

protected:
	TAttribute<EOrientation> Orientation;
	TAttribute<bool> Reverse;

	EOrientation CachedOrientation;
	bool CachedReverse;

	static int32 GetChildAlignment(const EOrientation InOrientation, const EFlowDirection InFlowDirection, const FSlot& InChildToArrange);
};
