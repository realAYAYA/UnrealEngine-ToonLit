// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/SequencerEditTool_Selection.h"
#include "Styling/AppStyle.h"
#include "SequencerCommonHelpers.h"
#include "SSequencer.h"
#include "ISequencerEditTool.h"
#include "ISequencerSection.h"
#include "SequencerHotspots.h"
#include "Tools/SequencerEntityVisitor.h"
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
	FSelectionPreviewVisitor(FSequencerSelectionPreview& InSelectionPreview, FSequencerSelection& InSelection, ESelectionPreviewState InSetStateTo, bool bInPinned)
		: SelectionPreview(InSelectionPreview)
		, ExistingSelection(InSelection.GetRawSelectedKeys())
		, SetStateTo(InSetStateTo)
		, bPinned(bInPinned)
	{}

	virtual void VisitKey(FKeyHandle KeyHandle, FFrameNumber KeyTime, const UE::Sequencer::TViewModelPtr<UE::Sequencer::FChannelModel>& Channel, UMovieSceneSection* Section) const override
	{
		using namespace UE::Sequencer;

		TSharedPtr<IPinnableExtension> PinnableItem = Channel->GetLinkedOutlinerItem().ImplicitCast();
		if (PinnableItem && PinnableItem->IsPinned() != bPinned)
		{
			return;
		}

		// Under default behavior keys have priority, so if a key is changing selection state then we remove any sections from the selection. The user can bypass this
		// by holding down the control key which will allow selecting both keys and sections.
		bool bKeySelectionHasPriority = !FSlateApplication::Get().GetModifierKeys().IsControlDown();
		bool bKeyIsSelected = ExistingSelection.Contains(KeyHandle);

		if (bKeySelectionHasPriority && 
			((bKeyIsSelected && SetStateTo == ESelectionPreviewState::NotSelected) ||
			(!bKeyIsSelected && SetStateTo == ESelectionPreviewState::Selected)))
		{
			// Clear selected models
			SelectionPreview.EmptyDefinedModelStates();
		}

		FSequencerSelectedKey Key(*Section, TSharedPtr<FChannelModel>(Channel), KeyHandle);
		SelectionPreview.SetSelectionState(Key, SetStateTo);
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
	ESelectionPreviewState SetStateTo;
	bool bPinned;
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

		Sequencer.GetSelection().SuspendBroadcast();

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
			Sequencer.GetSelection().EmptySelectedKeys();
			Sequencer.GetSelection().EmptySelectedTrackAreaItems();
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
		Walker.Traverse(FSelectionPreviewVisitor(SelectionPreview, Sequencer.GetSelection(), PreviewState, TrackArea.ShowPinned()), Sequencer.GetViewModel()->GetRootModel());
	}

	virtual void OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const UE::Sequencer::FVirtualTrackArea& VirtualTrackArea) override
	{
		// finish dragging the marquee selection
		auto& Selection = Sequencer.GetSelection();
		auto& SelectionPreview = Sequencer.GetSelectionPreview();

		// Patch everything from the selection preview into the actual selection
		for (const auto& Pair : SelectionPreview.GetDefinedKeyStates())
		{
			if (Pair.Value == ESelectionPreviewState::Selected)
			{
				// Select it in the main selection
				Selection.AddToSelection(Pair.Key);
			}
			else
			{
				Selection.RemoveFromSelection(Pair.Key);
			}
		}

		for (const auto& Pair : SelectionPreview.GetDefinedModelStates())
		{
			if (Pair.Value == ESelectionPreviewState::Selected)
			{
				// Select it in the main selection
				Selection.AddToSelection(Pair.Key.Pin());
			}
			else
			{
				Selection.RemoveFromSelection(Pair.Key.Pin());
			}
		}

		Selection.ResumeBroadcast();
		Selection.RequestOutlinerNodeSelectionChangedBroadcast();

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
			AllottedGeometry.ToPaintGeometry(SelectionTopLeft, SelectionBottomRight - SelectionTopLeft),
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
			AllottedGeometry.ToPaintGeometry(MousePosition + FVector2D(5, 5), CursorDecorator->ImageSize),
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
		TSharedPtr<ITrackAreaHotspot> Hotspot = Sequencer.GetViewModel()->GetTrackArea()->GetHotspot();
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
				DragOperation = MakeShareable( new FMarqueeDragOperation(Sequencer, TrackArea) );
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
}


void FSequencerEditTool_Selection::OnMouseCaptureLost()
{
	// Delaying nulling out until next tick because this could be invoked during OnMouseMove()
	GEditor->GetTimerManager()->SetTimerForNextTick([this]()
	{
		DelayedDrag.Reset();
		DragOperation = nullptr;
		CursorDecorator = nullptr;
	});
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
