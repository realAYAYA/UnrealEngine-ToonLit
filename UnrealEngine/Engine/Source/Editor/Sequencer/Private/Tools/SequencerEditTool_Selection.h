// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Tools/SequencerEditTool.h"

class SSequencer;

namespace UE
{
namespace Sequencer
{
	class STrackAreaView;
}
}

class FSequencerEditTool_Selection
	: public FSequencerEditTool
{
public:

	/** Static identifier for this edit tool */
	static const FName Identifier;

	/** Create and initialize a new instance. */
	FSequencerEditTool_Selection(FSequencer& InSequencer, UE::Sequencer::STrackAreaView& InTrackArea);

public:

	// ISequencerEditTool interface	
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;
	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(SWidget& OwnerWidget, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost() override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FName GetIdentifier() const override;
	virtual bool CanDeactivate() const override;
	virtual TSharedPtr<UE::Sequencer::ITrackAreaHotspot> GetDragHotspot() const override;
	virtual FReply OnKeyDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:

	/** Update the software cursor */
	void UpdateCursor(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Are we scrubbing time*/
	bool IsScrubTimeKeyEvent(const FKeyEvent& InKeyEvent);
private:

	/** Helper class responsible for handling delayed dragging */
	TOptional<UE::Sequencer::FDelayedDrag_Hotspot> DelayedDrag;
	
	/** Current drag operation if any */
	TSharedPtr<UE::Sequencer::ISequencerEditToolDragOperation> DragOperation;

	/** Cached mouse position for software cursor rendering */
	FVector2D MousePosition;

	/** TrackArea this object belongs to */
	UE::Sequencer::STrackAreaView& TrackArea;

	/** Software cursor decorator brush */
	const FSlateBrush* CursorDecorator;

	/** Whether or not we are scrubbing time*/
	bool bIsScrubbingTime = false;
};
