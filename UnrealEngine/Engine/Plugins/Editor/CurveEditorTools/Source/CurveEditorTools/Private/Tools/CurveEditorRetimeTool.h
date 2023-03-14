// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "ICurveEditorToolExtension.h"
#include "Layout/Geometry.h"
#include "Framework/DelayedDrag.h"
#include "Curves/KeyHandle.h"
#include "CurveDataAbstraction.h"
#include "CurveEditorTypes.h"
#include "CurveEditorSelection.h"
#include "CurveEditorRetimeTool.generated.h"

class FCurveEditor;
struct FCurveModelID;
struct FKeyHandle;


USTRUCT()
struct FCurveEditorRetimeAnchor
{
	GENERATED_BODY()

	FCurveEditorRetimeAnchor()
		: ValueInSeconds(0.0)
		, bIsSelected(false)
		, bIsHighlighted(false)
		, bIsCloseBtnHighlighted(false)
	{
	}

	FCurveEditorRetimeAnchor(double InSeconds)
		: ValueInSeconds(InSeconds)
		, bIsSelected(false)
		, bIsHighlighted(false)
		, bIsCloseBtnHighlighted(false)
	{
	}

	/** The time on the Timeline that this anchor is anchored at. */
	UPROPERTY()
	double ValueInSeconds;

	/** Is this anchor currently selected? */
	UPROPERTY()
	bool bIsSelected;

	/** Is this anchor currently highlighted? An anchor can be both selected and highlighted. */
	bool bIsHighlighted;

	/** Is the close button highlighted? */
	bool bIsCloseBtnHighlighted;

	// Calculate the geometry for this anchor based on the supplied geometry.
	void GetGeometry(const FGeometry& InWidgetGeometry, TSharedRef<FCurveEditor> InCurveEditor, FGeometry& OutBarGeometry, FGeometry& OutCloseButtonGeometry) const;
};

UCLASS()
class UCurveEditorRetimeToolData : public UObject
{
	GENERATED_BODY()

public:
	// List of anchor points. Assumes they're in order from lowest input time time to greatest.
	UPROPERTY()
	TArray<FCurveEditorRetimeAnchor> RetimingAnchors;
};

class FCurveEditorRetimeTool : public ICurveEditorToolExtension
{
public:
	FCurveEditorRetimeTool(TWeakPtr<FCurveEditor> InCurveEditor);
	~FCurveEditorRetimeTool();

	// ICurveEditorToolExtension
	virtual void OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 PaintOnLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(TSharedRef<SWidget> OwningWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(TSharedRef<SWidget> OwningWidget, const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(TSharedRef<SWidget> OwningWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(TSharedRef<SWidget> OwningWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void OnToolActivated() override;
	virtual void OnToolDeactivated() override;
	virtual void BindCommands(TSharedRef<FUICommandList> CommandBindings) override;
	// ~ICurveEditorToolExtension

private:
	virtual void DrawAnchor(const FCurveEditorRetimeAnchor& InAnchor, const FCurveEditorRetimeAnchor* OptionalNextAnchor, const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 PaintOnLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	virtual void OnDragStart();
	virtual void OnDrag(const FVector2D& InStartPosition, const FVector2D& InMousePosition);
	virtual void OnDragEnd();
	void StopDragIfPossible();

private:
	/** Weak pointer back to the owning curve editor. */
	TWeakPtr<FCurveEditor> WeakCurveEditor;

	/** The retiming data for this tool. This is a UObject for Undo/Redo purposes. Created and added to the root set on construction of the tool. */
	UCurveEditorRetimeToolData* RetimeData;

	/** The currently open transaction (if any) */
	TUniquePtr<class FScopedTransaction> ActiveTransaction;
	
	/** Set when attempting to move a drag handle. */
	TOptional<FDelayedDrag> DelayedDrag;

	struct FPreDragChannelData
	{
		FPreDragChannelData(FCurveModelID InCurveModel)
			: CurveID(InCurveModel)
		{
		}

		/** Array of all the handles in the section at the start of the drag */
		TArray<FKeyHandle> Handles;
		/** Array of all the above handle's times, one per index of Handles */
		TArray<FKeyPosition> FrameNumbers;
		/** Array of all the above handle's last dragged times, one per index of Handles */
		TArray<FKeyPosition> LastDraggedFrameNumbers;
		FCurveModelID CurveID;
	};

	struct FPreDragCurveData
	{
		/** Array of all the channels in the section before it was resized */
		TArray<FPreDragChannelData> CurveChannels;

		TArray<double> RetimeAnchorStartTimes;

	};

	/** Allocated when the drag starts and freed when the drag is finished. Used to do calculations based off of the original timing of the data. */
	TOptional<FPreDragCurveData> PreDragCurveData;

	/** Cached user selection for keys. We clear this when the tool is opened and restore it when they leave */
	TMap<FCurveModelID, FKeyHandleSet> CachedSelectionSet;
};