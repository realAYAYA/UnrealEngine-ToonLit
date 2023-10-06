// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AsyncDetailViewDiff.h"
#include "DiffUtils.h"
#include "Templates/SharedPointer.h"
#include "IDetailsView.h"

// Splitter that allows you to provide an FAsyncDetailViewDiff to connect like-properties between two or more details panels
class PROPERTYEDITOR_API SDetailsSplitter : public SCompoundWidget
{
public:
	class FSlot : public TSlotBase<FSlot>
	{
	public:
		FSlot() = default;

		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
			/** When the RuleSize is set to FractionOfParent, the size of the slot is the Value percentage of its parent size. */
			SLATE_ATTRIBUTE(float, Value)
			SLATE_ARGUMENT(TSharedPtr<IDetailsView>, DetailsView)
			SLATE_ATTRIBUTE(bool, IsReadonly) // default true
			SLATE_ATTRIBUTE(TSharedPtr<FAsyncDetailViewDiff>, DifferencesWithRightPanel)
		SLATE_SLOT_END_ARGS()
	};
	struct FPanel
	{
		TSharedPtr<IDetailsView> DetailsView;
		TAttribute<bool> IsReadonly;
		TAttribute<TSharedPtr<FAsyncDetailViewDiff>> DiffRight;
	};
	static FSlot::FSlotArguments Slot();
	
	SLATE_BEGIN_ARGS(SDetailsSplitter)
	{}
		SLATE_SLOT_ARGUMENT(FSlot, Slots)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	void AddSlot(const FSlot::FSlotArguments& SlotArgs, int32 Index = INDEX_NONE);
	FPanel& GetPanel(int32 Index);

private:
	
	enum class EPropertyCopyDirection
	{
		Copy_None,
		CopyLeftToRight,
		CopyRightToLeft,
	};
	
	struct FCopyPropertyButton
	{
		TWeakPtr<FDetailTreeNode> SourceDetailsNode;
		TWeakPtr<FDetailTreeNode> DestinationDetailsNode;
		ETreeDiffResult DiffResult = ETreeDiffResult::Invalid;
		EPropertyCopyDirection CopyDirection = EPropertyCopyDirection::Copy_None;
	};
	
	void PaintPropertyConnector(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FSlateRect& LeftPropertyRect, 
		const FSlateRect& RightPropertyRect, const FLinearColor& FillColor, const FLinearColor& OutlineColor) const;
	
	void PaintCopyPropertyButton(FSlateWindowElementList& OutDrawElements, int32 LayerId, const TUniquePtr<TDiffNode<TWeakPtr<FDetailTreeNode>>>& DiffNode,
		const FSlateRect& LeftPropertyRect, const FSlateRect& RightPropertyRect, EPropertyCopyDirection CopyDirection) const;
	
	TSharedPtr<SSplitter> Splitter;
	TArray<FPanel> Panels;
	FCopyPropertyButton HoveredCopyButton;
};
