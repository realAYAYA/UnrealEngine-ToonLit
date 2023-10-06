// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/SequencerEditTool_Selection.h"
#include "Styling/AppStyle.h"
#include "SequencerCommonHelpers.h"
#include "SSequencer.h"
#include "ISequencerEditTool.h"
#include "ISequencerSection.h"
#include "SequencerHotspots.h"
#include "IKeyArea.h"
#include "Tools/SequencerEntityVisitor.h"
#include "SequencerCommands.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/Views/ISequencerTreeView.h"
#include "MVVM/Views/ITrackAreaHotspot.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/VirtualTrackArea.h"

#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/Views/STrackAreaView.h"
#include "MVVM/Views/SOutlinerView.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/IPinnableExtension.h"
#include "MVVM/Extensions/ISelectableExtension.h"


struct FSelectionPreviewVisitor final
	: ISequencerEntityVisitor
{
	FSelectionPreviewVisitor(FSequencerSelectionPreview& InSelectionPreview, UE::Sequencer::FSequencerSelection& InSelection, ESelectionPreviewState InSetStateTo, bool bInPinned)
		: SelectionPreview(InSelectionPreview)
		, ExistingSelection(InSelection.KeySelection.GetSelected())
		, SetStateTo(InSetStateTo)
		, bPinned(bInPinned)
	{
		bIsControlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
	}

	virtual void VisitKeys(const UE::Sequencer::TViewModelPtr<UE::Sequencer::FChannelModel>& Channel, const TRange<FFrameNumber>& VisitRangeFrames) const override
	{
		using namespace UE::Sequencer;

		TSharedPtr<IPinnableExtension> PinnableItem = Channel->GetLinkedOutlinerItem().ImplicitCast();
		if (PinnableItem && PinnableItem->IsPinned() != bPinned)
		{
			return;
		}

		KeyHandlesScratch.Reset();
		Channel->GetKeyArea()->GetKeyInfo(&KeyHandlesScratch, nullptr, VisitRangeFrames);

		for (int32 Index = 0; Index < KeyHandlesScratch.Num(); ++Index)
		{
			// Under default behavior keys have priority, so if a key is changing selection state then we remove any sections from the selection. The user can bypass this
			// by holding down the control key which will allow selecting both keys and sections.
			bool bKeySelectionHasPriority = !bIsControlDown;
			bool bKeyIsSelected = ExistingSelection.Contains(KeyHandlesScratch[Index]);

			if (bKeySelectionHasPriority && 
				((bKeyIsSelected && SetStateTo == ESelectionPreviewState::NotSelected) ||
				(!bKeyIsSelected && SetStateTo == ESelectionPreviewState::Selected)))
			{
				// Clear selected models
				SelectionPreview.EmptyDefinedModelStates();
			}

			SelectionPreview.SetSelectionState(Channel, KeyHandlesScratch[Index], SetStateTo);
		}
	}

	virtual void VisitDataModel(UE::Sequencer::FViewModel* DataModel) const override
	{
		using namespace UE::Sequencer;

		ISelectableExtension* Selectable = DataModel->CastThis<ISelectableExtension>();
		if (!Selectable || !EnumHasAnyFlags(Selectable->IsSelectable(), ESelectionIntent::PersistentSelection))
		{
			return;
		}

		// If key selection has priority then we check to see if there are any keys selected. If there are key selected, we don't add this section.
		// Otherwise, we bypass this check
		bool bKeySelectionHasPriority = !FSlateApplication::Get().GetModifierKeys().IsControlDown();
		bool bKeyStateCheck = bKeySelectionHasPriority ? SelectionPreview.GetDefinedKeyStates().Num() == 0 : true;

		if (bKeyStateCheck)
		{
			SelectionPreview.SetSelectionState(DataModel->AsShared(), SetStateTo);
		}
	}

private:

	FSequencerSelectionPreview& SelectionPreview;
	const TSet<FKeyHandle>& ExistingSelection;
	mutable TArray<FKeyHandle> KeyHandlesScratch;
	ESelectionPreviewState SetStateTo;
	bool bPinned;
	bool bIsControlDown;
};

class FScrubTimeDragOperation
	: public UE::Sequencer::ISequencerEditToolDragOperation
{
public:

	FScrubTimeDragOperation(FSequencer& InSequencer, UE::Sequencer::STrackAreaView& InTrackArea)
		: Sequencer(InSequencer)
	{}

public:

	// ISequencerEditToolDragOperation interface

	virtual FCursorReply GetCursor() const override
	{
		return FCursorReply::Cursor(EMouseCursor::Default);
	}

	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const UE::Sequencer::FVirtualTrackArea& VirtualTrackArea) override
	{
		// Start a new marquee selection
		InitialPosition = CurrentPosition = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);
		CurrentMousePos = LocalMousePos;

		Sequencer.SetPlaybackStatus(EMovieScenePlayerStatus::Scrubbing);
		SequencerStartTime = Sequencer.GetLocalTime();
		InitialTime = VirtualTrackArea.PixelToSeconds(LocalMousePos.X);
	}

	virtual void OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const UE::Sequencer::FVirtualTrackArea& VirtualTrackArea) override
	{
		CurrentTime = VirtualTrackArea.PixelToSeconds(LocalMousePos.X);
		FFrameTime FrameTime = CalculateScrubTime();

		Sequencer.SnapSequencerTime(FrameTime);
		Sequencer.SetLocalTimeDirectly(FrameTime, true);
	}

	virtual void OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const UE::Sequencer::FVirtualTrackArea& VirtualTrackArea) override
	{
		Sequencer.SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
	}

	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override
	{
		return LayerId;
	}

private:

	FFrameTime CalculateScrubTime()
	{
		double Diff = CurrentTime - InitialTime;
		FFrameTime FrameTime= SequencerStartTime.Rate.AsFrameTime(Diff);
		FrameTime += SequencerStartTime.Time;
		return FrameTime;
	}

	/** The sequencer itself */
	FSequencer& Sequencer;

	FVector2D InitialPosition;
	FVector2D CurrentPosition;
	FVector2D CurrentMousePos;

	FQualifiedFrameTime SequencerStartTime;
	double InitialTime;
	double CurrentTime;
};

class FMarqueeDragOperation
	: public UE::Sequencer::ISequencerEditToolDragOperation
{
public:

	FMarqueeDragOperation(FSequencer& InSequencer, UE::Sequencer::STrackAreaView& InTrackArea)
		: Sequencer(InSequencer)
		, TrackArea(InTrackArea)
		, SequencerWidget(StaticCastSharedRef<SSequencer>(InSequencer.GetSequencerWidget()))
		, PreviewState(ESelectionPreviewState::Selected)
	{}

public:

	// ISequencerEditToolDragOperation interface

	virtual FCursorReply GetCursor() const override
	{
		return FCursorReply::Cursor( EMouseCursor::Default );
	}

	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const UE::Sequencer::FVirtualTrackArea& VirtualTrackArea) override
	{
		// Start a new marquee selection
		InitialPosition = CurrentPosition = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);
		CurrentMousePos = LocalMousePos;

		EventSuppressor = Sequencer.GetViewModel()->GetSelection()->SuppressEventsLongRunning();

		if (MouseEvent.IsShiftDown())
		{
			PreviewState = ESelectionPreviewState::Selected;
		}
		else if (MouseEvent.IsAltDown())
		{
			PreviewState = ESelectionPreviewState::NotSelected;
		}
		else
		{
			PreviewState = ESelectionPreviewState::Selected;

			// @todo: selection in transactions
			//leave selections in the tree view alone so that dragging operations act similarly to click operations which don't change treeview selection state.
			Sequencer.GetViewModel()->GetSelection()->KeySelection.Empty();
			Sequencer.GetViewModel()->GetSelection()->TrackArea.Empty();
		}
	}

	virtual void OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const UE::Sequencer::FVirtualTrackArea& VirtualTrackArea) override
	{
		using namespace UE::Sequencer;

		// hange the current marquee selection
		const FVector2D MouseDelta = MouseEvent.GetCursorDelta();

		// Handle virtual scrolling when at the vertical extremes of the widget (performed before we clamp the mouse pos)
		{
			const float ScrollThresholdV = VirtualTrackArea.GetPhysicalSize().Y * 0.025f;

			TSharedPtr<SOutlinerView> TreeView = TrackArea.GetOutliner().Pin();

			float Difference = LocalMousePos.Y - ScrollThresholdV;
			if (Difference < 0 && MouseDelta.Y < 0)
			{
				TreeView->ScrollByDelta( Difference * 0.1f );
			}

			Difference = LocalMousePos.Y - (VirtualTrackArea.GetPhysicalSize().Y - ScrollThresholdV);
			if (Difference > 0 && MouseDelta.Y > 0)
			{
				TreeView->ScrollByDelta( Difference * 0.1f) ;
			}
		}

		// Clamp the vertical position to the actual bounds of the track area
		LocalMousePos.Y = FMath::Clamp(LocalMousePos.Y, 0.f, VirtualTrackArea.GetPhysicalSize().Y);
		CurrentPosition = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);

		// Clamp software cursor position to bounds of the track area
		CurrentMousePos = LocalMousePos;
		CurrentMousePos.X = FMath::Clamp(CurrentMousePos.X, 0.f, VirtualTrackArea.GetPhysicalSize().X);

		TRange<double> ViewRange = Sequencer.GetViewRange();

		// Handle virtual scrolling when at the horizontal extremes of the widget
		{
			const double ScrollThresholdH = ViewRange.Size<double>() * 0.025f;

			double Difference = CurrentPosition.X - (ViewRange.GetLowerBoundValue() + ScrollThresholdH);
			if (Difference < 0 && MouseDelta.X < 0)
			{
				Sequencer.StartAutoscroll(Difference);
			}
			else
			{
				Difference = CurrentPosition.X - (ViewRange.GetUpperBoundValue() - ScrollThresholdH);
				if (Difference > 0 && MouseDelta.X > 0)
				{
					Sequencer.StartAutoscroll(Difference);
				}
				else
				{
					Sequencer.StopAutoscroll();
				}
			}
		}

		// Calculate the size of a key in virtual space
		FVector2D VirtualKeySize;
		VirtualKeySize.X = SequencerSectionConstants::KeySize.X / VirtualTrackArea.GetPhysicalSize().X * ViewRange.Size<float>();
		// Vertically, virtual units == physical units
		VirtualKeySize.Y = SequencerSectionConstants::KeySize.Y;

		// Visit everything using the preview selection primarily as well as the 
		FSequencerSelectionPreview& SelectionPreview = Sequencer.GetSelectionPreview();

		// Ensure the preview is empty before calculating the intersection
		SelectionPreview.Empty();

		// Now walk everything within the current marquee range, setting preview selection states as we go
		FSequencerEntityWalker Walker(FSequencerEntityRange(TopLeft(), BottomRight(), VirtualTrackArea.GetTickResolution()), VirtualKeySize);
		Walker.Traverse(FSelectionPreviewVisitor(SelectionPreview, *Sequencer.GetViewModel()->GetSelection(), PreviewState, TrackArea.ShowPinned()), Sequencer.GetViewModel()->GetRootModel());
	}

	virtual void OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const UE::Sequencer::FVirtualTrackArea& VirtualTrackArea) override
	{
		using namespace UE::Sequencer;

		// finish dragging the marquee selection
		FSequencerSelection& Selection = *Sequencer.GetViewModel()->GetSelection();
		FSequencerSelectionPreview& SelectionPreview = Sequencer.GetSelectionPreview();

		// Patch everything from the selection preview into the actual selection
		for (const TPair<FKeyHandle, ESelectionPreviewState>& Pair : SelectionPreview.GetDefinedKeyStates())
		{
			if (Pair.Value == ESelectionPreviewState::Selected)
			{
				Selection.KeySelection.Select(SelectionPreview.GetChannelForKey(Pair.Key), Pair.Key);
			}
			else
			{
				Selection.KeySelection.Deselect(Pair.Key);
			}
		}

		for (const TPair<TWeakPtr<FViewModel>, ESelectionPreviewState>& Pair : SelectionPreview.GetDefinedModelStates())
		{
			if (TSharedPtr<FViewModel> Model = Pair.Key.Pin())
			{
				if (TViewModelPtr<IOutlinerExtension> OutlinerItem = CastViewModel<IOutlinerExtension>(Model))
				{
					Selection.Outliner.Select(OutlinerItem);
				}
				else
				{
					Selection.TrackArea.Select(Model);
				}
			}
		}

		// Broadcast selection events
		EventSuppressor = nullptr;

		// We're done with this now
		SelectionPreview.Empty();
	}

	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override
	{
		using namespace UE::Sequencer;

		// convert to physical space for rendering
		const FVirtualTrackArea VirtualTrackArea = SequencerWidget->GetVirtualTrackArea(&TrackArea);

		FVector2D SelectionTopLeft = VirtualTrackArea.VirtualToPhysical(TopLeft());
		FVector2D SelectionBottomRight = VirtualTrackArea.VirtualToPhysical(BottomRight());

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(SelectionBottomRight - SelectionTopLeft, FSlateLayoutTransform(SelectionTopLeft)),
			FAppStyle::GetBrush(TEXT("MarqueeSelection"))
			);

		return LayerId + 1;
	}

private:

	FVector2D TopLeft() const
	{
		return FVector2D(
			FMath::Min(InitialPosition.X, CurrentPosition.X),
			FMath::Min(InitialPosition.Y, CurrentPosition.Y)
		);
	}
	
	FVector2D BottomRight() const
	{
		return FVector2D(
			FMath::Max(InitialPosition.X, CurrentPosition.X),
			FMath::Max(InitialPosition.Y, CurrentPosition.Y)
		);
	}

	/** The sequencer itself */
	FSequencer& Sequencer;

	UE::Sequencer::STrackAreaView& TrackArea;

	TUniquePtr<UE::Sequencer::FSelectionEventSuppressor> EventSuppressor;

	/** Sequencer widget */
	TSharedRef<SSequencer> SequencerWidget;

	/** Whether we should select/deselect things in this marquee operation */
	ESelectionPreviewState PreviewState;

	FVector2D InitialPosition;
	FVector2D CurrentPosition;
	FVector2D CurrentMousePos;
};


const FName FSequencerEditTool_Selection::Identifier = "Selection";

FSequencerEditTool_Selection::FSequencerEditTool_Selection(FSequencer& InSequencer, UE::Sequencer::STrackAreaView& InTrackArea)
	: FSequencerEditTool(InSequencer)
	, TrackArea(InTrackArea)
	, CursorDecorator(nullptr)
{ }


FCursorReply FSequencerEditTool_Selection::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (DragOperation)
	{
		return DragOperation->GetCursor();
	}

	return FCursorReply::Cursor(EMouseCursor::Crosshairs);
}


int32 FSequencerEditTool_Selection::OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (DragOperation.IsValid())
	{
		LayerId = DragOperation->OnPaint(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
	}

	if (CursorDecorator)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(CursorDecorator->ImageSize, FSlateLayoutTransform(MousePosition + FVector2D(5.f, 5.f))),
			CursorDecorator
			);
	}

	return LayerId;
}


FReply FSequencerEditTool_Selection::OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	using namespace UE::Sequencer;

	UpdateCursor(MyGeometry, MouseEvent);

	DelayedDrag.Reset();

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = Sequencer.GetViewModel()->CastThisShared<FSequencerEditorViewModel>();
		TSharedPtr<ITrackAreaHotspot> Hotspot = SequencerViewModel->GetHotspot();
		DelayedDrag = FDelayedDrag_Hotspot(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), EKeys::LeftMouseButton, Hotspot);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


FReply FSequencerEditTool_Selection::OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	using namespace UE::Sequencer;

	UpdateCursor(MyGeometry, MouseEvent);

	if (DelayedDrag.IsSet())
	{
		FReply Reply = FReply::Handled();

		TSharedRef<SSequencer> SequencerWidget = StaticCastSharedRef<SSequencer>(Sequencer.GetSequencerWidget());
		const FVirtualTrackArea VirtualTrackArea = SequencerWidget->GetVirtualTrackArea(&TrackArea);

		if (DragOperation.IsValid())
		{
			FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			DragOperation->OnDrag(MouseEvent, LocalPosition, VirtualTrackArea );
		}
		else if (DelayedDrag->AttemptDragStart(MouseEvent))
		{
			if (DelayedDrag->Hotspot.IsValid())
			{
				// We only allow resizing with the marquee selection tool enabled
				if (DelayedDrag->Hotspot->CastThis<FSectionHotspot>() == nullptr && DelayedDrag->Hotspot->CastThis<FKeyHotspot>() == nullptr)
				{
					DragOperation = DelayedDrag->Hotspot->InitiateDrag(MouseEvent);
				}
			}

			if (!DragOperation.IsValid())
			{
				if (bIsScrubbingTime)
				{
					DragOperation = MakeShareable(new FScrubTimeDragOperation(Sequencer, TrackArea));
				}
				else
				{
					DragOperation = MakeShareable(new FMarqueeDragOperation(Sequencer, TrackArea));
				}
			}

			if (DragOperation.IsValid())
			{
				DragOperation->OnBeginDrag(MouseEvent, DelayedDrag->GetInitialPosition(), VirtualTrackArea);

				// Steal the capture, as we're now the authoritative widget in charge of a mouse-drag operation
				Reply.CaptureMouse(OwnerWidget.AsShared());
			}
		}

		return Reply;
	}
	return FReply::Unhandled();
}


FReply FSequencerEditTool_Selection::OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateCursor(MyGeometry, MouseEvent);

	DelayedDrag.Reset();
	if (DragOperation.IsValid())
	{
		TSharedRef<SSequencer> SequencerWidget = StaticCastSharedRef<SSequencer>(Sequencer.GetSequencerWidget());
		FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		DragOperation->OnEndDrag(MouseEvent, LocalPosition, SequencerWidget->GetVirtualTrackArea(&TrackArea));
		DragOperation = nullptr;

		CursorDecorator = nullptr;

		Sequencer.StopAutoscroll();
		return FReply::Handled().ReleaseMouseCapture();
	}
	else
	{
		SequencerHelpers::PerformDefaultSelection(Sequencer, MouseEvent);

		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && !Sequencer.IsReadOnly())
		{
			TSharedPtr<SWidget> MenuContent = SequencerHelpers::SummonContextMenu( Sequencer, MyGeometry, MouseEvent );
			if (MenuContent.IsValid())
			{
				FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

				FSlateApplication::Get().PushMenu(
					OwnerWidget.AsShared(),
					WidgetPath,
					MenuContent.ToSharedRef(),
					MouseEvent.GetScreenSpacePosition(),
					FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
					);

				return FReply::Handled().SetUserFocus(MenuContent.ToSharedRef(), EFocusCause::SetDirectly).ReleaseMouseCapture();
			}
		}

		return FReply::Handled();
	}
}


void FSequencerEditTool_Selection::OnMouseLeave(SWidget& OwnerWidget, const FPointerEvent& MouseEvent)
{
	if (!DragOperation.IsValid())
	{
		CursorDecorator = nullptr;
	}
	bIsScrubbingTime = false;
}


void FSequencerEditTool_Selection::OnMouseCaptureLost()
{
	// Delaying nulling out until next tick because this could be invoked during OnMouseMove()
	GEditor->GetTimerManager()->SetTimerForNextTick([this]()
	{
		DelayedDrag.Reset();
		DragOperation = nullptr;
		CursorDecorator = nullptr;
		bIsScrubbingTime = false;
	});
}

bool FSequencerEditTool_Selection::IsScrubTimeKeyEvent(const FKeyEvent& InKeyEvent)
{
	const FSequencerCommands& Commands = FSequencerCommands::Get();
	// Need to iterate through primary and secondary to make sure they are all pressed.
	for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
	{
		EMultipleKeyBindingIndex ChordIndex = static_cast<EMultipleKeyBindingIndex>(i);
		const FInputChord& Chord = *Commands.ScrubTimeViewport->GetActiveChord(ChordIndex);
		const bool bIsMovingTimeSlider = Chord.IsValidChord() && InKeyEvent.GetKey() == Chord.Key;
		if (bIsMovingTimeSlider)
		{
			return true;
		}
	}
	return false;
}

FReply FSequencerEditTool_Selection::OnKeyDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsScrubTimeKeyEvent(InKeyEvent))
	{
		bIsScrubbingTime = true;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply FSequencerEditTool_Selection::OnKeyUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsScrubTimeKeyEvent(InKeyEvent) && bIsScrubbingTime)
	{
		bIsScrubbingTime = false;
		//would be nice to cancle the drag but doesn't seem like we can like we can with curve editor drag handlers.
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FName FSequencerEditTool_Selection::GetIdentifier() const
{
	return Identifier;
}

bool FSequencerEditTool_Selection::CanDeactivate() const
{
	return !DelayedDrag.IsSet();
}

TSharedPtr<UE::Sequencer::ITrackAreaHotspot> FSequencerEditTool_Selection::GetDragHotspot() const
{
	return DelayedDrag.IsSet() ? DelayedDrag->Hotspot : nullptr;
}

void FSequencerEditTool_Selection::UpdateCursor(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// Don't update the brush if we have a drag operation
	if (!DragOperation.IsValid())
	{
		if (MouseEvent.IsShiftDown())
		{
			CursorDecorator = FAppStyle::Get().GetBrush(TEXT("Sequencer.CursorDecorator_MarqueeAdd"));
		}
		else if (MouseEvent.IsAltDown())
		{
			CursorDecorator = FAppStyle::Get().GetBrush(TEXT("Sequencer.CursorDecorator_MarqueeSubtract"));
		}
		else
		{
			CursorDecorator = nullptr;
		}
	}
}
