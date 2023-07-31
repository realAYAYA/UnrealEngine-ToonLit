// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/SOutlinerView.h"

#include "Algo/BinarySearch.h"
#include "Containers/ArrayView.h"
#include "Framework/Layout/Overscroll.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Views/ITypedTableView.h"
#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/ChildrenBase.h"
#include "Layout/Geometry.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "MVVM/Extensions/IDraggableOutlinerExtension.h"
#include "MVVM/Extensions/IDroppableExtension.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/IPinnableExtension.h"
#include "MVVM/Extensions/ISelectableExtension.h"
#include "MVVM/Extensions/ITrackAreaExtension.h"
#include "MVVM/ViewModels/OutlinerSpacer.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModelDragDropOp.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/Views/IOutlinerSelectionHandler.h"
#include "MVVM/Views/STrackAreaView.h"
#include "MVVM/Views/STrackLane.h"
#include "MVVM/Views/TreeViewTraits.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateLayoutTransform.h"
#include "ScopedTransaction.h"
#include "SequencerCoreFwd.h"
#include "Styling/SlateBrush.h"
#include "Styling/WidgetStyle.h"
#include "Templates/Tuple.h"
#include "Types/SlateStructs.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SHeaderRow.h"

class FPaintArgs;
class FSlateRect;
class ITableRow;
class SWidget;
namespace UE::Sequencer { class FEditorViewModel; }
namespace UE::Sequencer { class FOutlinerSpacer; }

namespace UE
{
namespace Sequencer
{

const FName SOutlinerView::TrackNameColumn("TrackArea");

void SOutlinerViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, TWeakViewModelPtr<IOutlinerExtension> InWeakModel)
{
	WeakModel = InWeakModel;

	OnDetectDrag = InArgs._OnDetectDrag;
	OnGenerateWidgetForColumn = InArgs._OnGenerateWidgetForColumn;

	SMultiColumnTableRow::Construct(
		SMultiColumnTableRow::FArguments()
			.OnDragDetected(this, &SOutlinerViewRow::OnDragDetected)
			.OnCanAcceptDrop(this, &SOutlinerViewRow::OnCanAcceptDrop)
			.OnAcceptDrop(this, &SOutlinerViewRow::OnAcceptDrop)
			.ShowSelection(IsSelectable())
			.Padding(FMargin(0.f)),
		OwnerTableView);
}

SOutlinerViewRow::~SOutlinerViewRow()
{
}

TSharedRef<SWidget> SOutlinerViewRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	TViewModelPtr<IOutlinerExtension> DataModel = WeakModel.Pin();
	return DataModel ? OnGenerateWidgetForColumn.Execute(DataModel, ColumnId, SharedThis(this)) : SNullWidget::NullWidget;
}

void SOutlinerViewRow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	StaticCastSharedPtr<SOutlinerView>(OwnerTablePtr.Pin())->ReportChildRowGeometry(WeakModel.Pin(), AllottedGeometry);
}

bool SOutlinerViewRow::IsSelectable() const
{
	TSharedPtr<ISelectableExtension> Selectable = WeakModel.ImplicitPin();
	return !Selectable || Selectable->IsSelectable() != ESelectionIntent::Never;
}

FReply SOutlinerViewRow::OnDragDetected( const FGeometry& InGeometry, const FPointerEvent& InPointerEvent )
{
	if (OnDetectDrag.IsBound())
	{
		return OnDetectDrag.Execute(InGeometry, InPointerEvent, SharedThis(this));
	}
	return FReply::Unhandled();
}

int32 SOutlinerViewRow::OnPaintDropIndicator(EItemDropZone InItemDropZone, const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Draw feedback for user dropping an item above, below, or onto a row.
	const FSlateBrush* DropIndicatorBrush = GetDropIndicatorBrush(InItemDropZone);

	// Offset by the indentation amount
	static float OffsetX = 10.0f;
	FVector2D Offset(OffsetX * GetIndentLevel(), 0.f);
	FSlateDrawElement::MakeBox
	(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.GetLocalSize() - Offset), FSlateLayoutTransform(Offset)),
		DropIndicatorBrush,
		ESlateDrawEffect::None,
		DropIndicatorBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
	);

	return LayerId;
}

TOptional<EItemDropZone> SOutlinerViewRow::OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone, TWeakViewModelPtr<IOutlinerExtension> InDataModel)
{
	TViewModelPtr<IOutlinerExtension> ThisModel = InDataModel.Pin();
	if (!ThisModel)
	{
		return TOptional<EItemDropZone>();
	}
	else if (InItemDropZone == EItemDropZone::BelowItem && ThisModel->IsExpanded())
	{
		// Cannot drop immediately below items that are expanded since
		// it looks like you are dropping into the item, but the object
		// will end up below all its expanded children.
		return TOptional<EItemDropZone>();
	}

	// The model we are indirectly dropping into (either this model, or its parent)
	FViewModelPtr DropModel = ThisModel;
	// The model we are directly interacting with - only set when attaching before/after
	FViewModelPtr TargetModel;

	if (!DropModel)
	{
		return TOptional<EItemDropZone>();
	}

	// When dropping above or below an item, we always forward to its parent
	if (InItemDropZone == EItemDropZone::AboveItem || InItemDropZone == EItemDropZone::BelowItem)
	{
		TargetModel = DropModel;
		DropModel = DropModel->CastParent<IOutlinerDropTargetOutlinerExtension>();
	}

	TViewModelPtr<IOutlinerDropTargetOutlinerExtension> DropTarget = DropModel.ImplicitCast();
	if (!DropTarget)
	{
		return TOptional<EItemDropZone>();
	}

	return DropTarget->CanAcceptDrop(TargetModel, DragDropEvent, InItemDropZone);
}

FReply SOutlinerViewRow::OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone, TWeakViewModelPtr<IOutlinerExtension> InDataModel)
{
	TViewModelPtr<IOutlinerExtension> ThisModel = InDataModel.Pin();
	if (!ThisModel)
	{
		return FReply::Unhandled();
	}
	else if (InItemDropZone == EItemDropZone::BelowItem && ThisModel->IsExpanded())
	{
		// Cannot drop immediately below items that are expanded since
		// it looks like you are dropping into the item, but the object
		// will end up below all its expanded children.
		return FReply::Unhandled();
	}

	// The model we are indirectly dropping into (either this model, or its parent)
	FViewModelPtr DropModel = ThisModel;
	// The model we are directly interacting with - only set when attaching before/after
	FViewModelPtr TargetModel;

	// When dropping above or below an item, we always forward to its parent
	if (InItemDropZone == EItemDropZone::AboveItem || InItemDropZone == EItemDropZone::BelowItem)
	{
		TargetModel = DropModel;
		DropModel = DropModel->CastParent<IOutlinerDropTargetOutlinerExtension>();
	}

	TViewModelPtr<IOutlinerDropTargetOutlinerExtension> DropTarget = DropModel.ImplicitCast();
	if (DropTarget)
	{
		DropTarget->PerformDrop(TargetModel, DragDropEvent, InItemDropZone);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TViewModelPtr<IOutlinerExtension> SOutlinerViewRow::GetDataModel() const
{
	return WeakModel.Pin();
}

TSharedPtr<STrackLane> SOutlinerViewRow::GetTrackLane(bool bOnlyOwnTrackLane) const
{
	if (!bOnlyOwnTrackLane)
	{
		// Return the track lane, regardless of it being created by our own outliner item, or
		// being a reference to a parent outliner item's track lane.
		return TrackLane;
	}
	if (TrackLane && TrackLane->GetOutlinerItem() == GetDataModel())
	{
		// Return the track lane only if it was created by our own outliner item.
		return TrackLane;
	}
	return nullptr;
}

void SOutlinerViewRow::SetTrackLane(const TSharedPtr<STrackLane>& InTrackLane)
{
	TrackLane = InTrackLane;
}

const FSlateBrush* SOutlinerViewRow::GetBorder() const 
{
	TSharedPtr<IOutlinerExtension> OutlinerItem = WeakModel.ImplicitPin();
	if (OutlinerItem && !OutlinerItem->HasBackground())
	{
		return nullptr;
	}

	return SMultiColumnTableRow::GetBorder();
}

void SOutlinerView::Construct(const FArguments& InArgs, TWeakPtr<FOutlinerViewModel> InWeakOutliner, const TSharedRef<STrackAreaView>& InTrackArea)
{
	WeakOutliner = InWeakOutliner;
	TrackArea = InTrackArea;
	bUpdatingTreeSelection = false;
	bRightMouseButtonDown = false;
	bShowPinnedNodes = false;
	bSelectionChangesPending = false;
	bRefreshPhysicalGeometry = false;
	bForwardToSelectionHandler = true;

	SelectionHandlerAttribute = InArgs._SelectionHandler;

	HeaderRow = SNew(SHeaderRow).Visibility(EVisibility::Collapsed);

	SetupColumns(InArgs);

	WeakOutliner.Pin()->OnRefreshed.AddSP(this, &SOutlinerView::Refresh);

	STreeView::Construct
	(
		STreeView::FArguments()
		.TreeItemsSource(&RootNodes)
		.OnItemToString_Debug(this, &SOutlinerView::OnItemToString_Debug)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SOutlinerView::OnGenerateRow)
		.OnGetChildren(this, &SOutlinerView::OnGetChildren)
		.HeaderRow(HeaderRow)
		.ExternalScrollbar(InArgs._ExternalScrollbar)
		.OnExpansionChanged(this, &SOutlinerView::OnExpansionChanged)
		.OnTreeViewScrolled(this, &SOutlinerView::HandleTableViewScrolled)
		.AllowOverscroll(EAllowOverscroll::No)
		.OnContextMenuOpening( this, &SOutlinerView::OnContextMenuOpening )
		.OnSetExpansionRecursive(this, &SOutlinerView::SetItemExpansionRecursive)
		.HighlightParentNodesForSelection(true)
		.AllowInvisibleItemSelection(true)  //without this we deselect everything when we filter or we collapse, etc..
	);
}

SOutlinerView::~SOutlinerView()
{
	TrackArea.Reset();
	PinnedTreeViews.Empty();
}

void SOutlinerView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const bool bWasPendingRefresh = IsPendingRefresh();

	STreeView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bWasPendingRefresh || bRefreshPhysicalGeometry)
	{
		UpdatePhysicalGeometry(true);
		bRefreshPhysicalGeometry = false;
	}
}

TSharedPtr<FOutlinerViewModel> SOutlinerView::GetOutlinerModel() const
{
	return WeakOutliner.Pin();
}

FString SOutlinerView::OnItemToString_Debug(TWeakViewModelPtr<IOutlinerExtension> InWeakModel)
{
	TViewModelPtr<IOutlinerExtension> Model = InWeakModel.ImplicitPin();
	return Model ? Model->GetLabel().ToString() : FString();
}

int32 SOutlinerView::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	return STreeView::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FReply SOutlinerView::OnDragRow(const FGeometry&, const FPointerEvent&, TSharedRef<SOutlinerViewRow> InRow)
{
	TSharedPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin();
	if (!Outliner)
	{
		return FReply::Unhandled();
	}

	TArray<TWeakViewModelPtr<IOutlinerExtension>> Selection = GetSelectedItems();
	if (Selection.Num() > 0)
	{
		for (int32 Index = Selection.Num()-1; Index >= 0; --Index)
		{
			TSharedPtr<IDraggableOutlinerExtension> Draggable = Selection[Index].ImplicitPin();
			if (!Draggable || !Draggable->CanDrag())
			{
				// Order is not important so we can opt for performance with RemoveAtSwap
				Selection.RemoveAtSwap(Index, 1, false);
			}
		}

		// If there were no nodes selected, don't start a drag drop operation.
		if (Selection.Num() == 0)
		{
			return FReply::Unhandled();
		}

		TSharedRef<FDragDropOperation> DragDropOp = Outliner->InitiateDrag( MoveTemp(Selection) );
		return FReply::Handled().BeginDragDrop( DragDropOp );
	}

	return FReply::Unhandled();
}

void SOutlinerView::ReportChildRowGeometry(const TViewModelPtr<IOutlinerExtension>& InNode, const FGeometry& InGeometry)
{
	FCachedGeometry* PhysicalNode = PhysicalNodes.FindByPredicate(
			[InNode](FCachedGeometry& Item) { return Item.WeakItem == InNode; });
	if (PhysicalNode)
	{
		PhysicalNode->PhysicalHeight = InGeometry.Size.Y;
	}
}

void SOutlinerView::GetVisibleItems(TArray<TViewModelPtr<IOutlinerExtension>>& OutItems) const
{
	OutItems.Reserve(OutItems.Num() + PhysicalNodes.Num());
	for (const FCachedGeometry& CachedGeometry : PhysicalNodes)
	{
		if (TViewModelPtr<IOutlinerExtension> Item = CachedGeometry.WeakItem.Pin())
		{
			OutItems.Add(Item);
		}
	}
}

void SOutlinerView::ForceSetSelectedItems(const TSet<TWeakPtr<FViewModel>>& InItems)
{
	TGuardValue<bool> Guard(bForwardToSelectionHandler, false);

	Private_ClearSelection();
	for (const TWeakPtr<FViewModel>& Item : InItems)
	{
		TWeakViewModelPtr<IOutlinerExtension> WeakItem(Item);
		if (WeakItem.Pin())
		{
			Private_SetItemSelection(WeakItem, true, false);
		}
	}

	Private_SignalSelectionChanged(ESelectInfo::Direct);
}

TViewModelPtr<IOutlinerExtension> SOutlinerView::HitTestNode(float InPhysical) const
{
	// Find the first node with a top after the specified value - the hit node must be the one preceeding this
	const int32 FoundIndex = Algo::UpperBoundBy(PhysicalNodes, InPhysical, &FCachedGeometry::PhysicalTop) - 1;
	if (FoundIndex >= 0)
	{
		return PhysicalNodes[FoundIndex].WeakItem.Pin();
	}

	return nullptr;
}

float SOutlinerView::PhysicalToVirtual(float InPhysical) const
{
	// Find the first node with a top after the specified value - the hit node must be the one preceeding this
	const int32 FoundIndex = Algo::UpperBoundBy(PhysicalNodes, InPhysical, &FCachedGeometry::PhysicalTop) - 1;
	if (FoundIndex >= 0)
	{
		FCachedGeometry Found = PhysicalNodes[FoundIndex];
		const float FractionalHeight = (Found.PhysicalHeight != 0.f) ? (InPhysical - Found.PhysicalTop) / Found.PhysicalHeight : 0.f;
		return Found.VirtualTop + Found.VirtualHeight * FractionalHeight;
	}

	if (PhysicalNodes.Num())
	{
		const FCachedGeometry& First = PhysicalNodes[0];
		if (InPhysical < First.PhysicalTop)
		{
			return First.VirtualTop + (InPhysical - First.PhysicalTop);
		}
		else
		{
			const FCachedGeometry& Last = PhysicalNodes.Last();
			return First.VirtualTop + (InPhysical - Last.PhysicalTop);
		}
	}

	return InPhysical;
}

float SOutlinerView::VirtualToPhysical(float InVirtual) const
{
	// Find the first node with a top after the specified value - the hit node must be the one preceeding this
	int32 FoundIndex = Algo::UpperBoundBy(PhysicalNodes, InVirtual, &FCachedGeometry::VirtualTop) - 1;

	if (FoundIndex >= 0)
	{
		const FCachedGeometry& Found = PhysicalNodes[FoundIndex];
		const float FractionalHeight = (Found.VirtualHeight != 0.f) ? (InVirtual - Found.VirtualTop) / Found.VirtualHeight : 0.f;
		return Found.PhysicalTop + Found.PhysicalHeight * FractionalHeight;
	}

	if (PhysicalNodes.Num())
	{
		const FCachedGeometry& Last = PhysicalNodes.Last();
		return Last.PhysicalTop + (InVirtual - Last.VirtualTop);
	}

	return InVirtual;
}

void SOutlinerView::SetupColumns(const FArguments& InArgs)
{
	TSharedPtr<FEditorViewModel> EditorViewModel = WeakOutliner.Pin()->GetEditor();

	// Define a column for the Outliner
	auto GenerateOutliner = [=](const TWeakViewModelPtr<IOutlinerExtension>& InWeakModel, const TSharedRef<SOutlinerViewRow>& InRow) -> TSharedRef<SWidget>
	{
		if (TSharedPtr<IOutlinerExtension> OutlinerItem = InWeakModel.ImplicitPin())
		{
			return OutlinerItem->CreateOutlinerView(FCreateOutlinerViewParams{ InRow, EditorViewModel });
		}

		ensureMsgf(false, TEXT("Attempting to create an outliner widget for a view model that is either dead, or not an outliner item."));
		return SNew(SBox).HeightOverride(10.f);
	};

	Columns.Add("Outliner", FOutlinerViewColumn(GenerateOutliner, 1.f));

	// Now populate the header row with the columns
	for (TTuple<FName, FOutlinerViewColumn>& Pair : Columns)
	{
		if (Pair.Key != TrackNameColumn)
		{
			HeaderRow->AddColumn(
				SHeaderRow::Column(Pair.Key)
				.FillWidth(Pair.Value.Width)
			);
		}
	}
}

void SOutlinerView::UpdateTrackArea()
{
	// Add or remove the column
	if (const FOutlinerViewColumn* Column = Columns.Find(TrackNameColumn))
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(TrackNameColumn)
			.FillWidth(Column->Width)
		);
	}
}

void SOutlinerView::AddPinnedTreeView(TSharedPtr<SOutlinerView> PinnedTreeView)
{
	PinnedTreeViews.Add(PinnedTreeView);
	PinnedTreeView->SetPrimaryTreeView(SharedThis(this));
}

void SOutlinerView::OnRightMouseButtonDown(const FPointerEvent& MouseEvent)
{
	STreeView::OnRightMouseButtonDown(MouseEvent);
	bRightMouseButtonDown = true;
}

void SOutlinerView::OnRightMouseButtonUp(const FPointerEvent& MouseEvent)
{
	STreeView::OnRightMouseButtonUp(MouseEvent);
	bRightMouseButtonDown = false;
}

FReply SOutlinerView::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	FReply Result = STreeView::OnDragOver(MyGeometry, DragDropEvent);

	if (!Result.IsEventHandled())
	{
		TSharedPtr<FOutlinerViewModelDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FOutlinerViewModelDragDropOp>();
		if (DragDropOp.IsValid())
		{
			// Reset tooltip to indicate that the dragged objects can be moved to the root (ie. unparented)
			DragDropOp->ResetToDefaultToolTip();
		}

		return Result.Handled();
	}
	return Result;
}

FReply SOutlinerView::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin();
	if (!Outliner)
	{
		return FReply::Handled();
	}

	FReply Result = STreeView::OnDrop(MyGeometry, DragDropEvent);

	TSharedPtr<FOutlinerViewModelDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FOutlinerViewModelDragDropOp>();
	if (!Result.IsEventHandled() && DragDropOp.IsValid() && DragDropOp->GetDraggedViewModels().Num())
	{
		const FScopedTransaction Transaction(NSLOCTEXT("SequencerTrackNode", "MoveItems", "Move items."));

		TViewModelPtr<IDroppableExtension> Root = Outliner->GetRootItem().ImplicitCast();
		if (Root)
		{
			Root->ProcessDragOperation(*DragDropOp);
		}

		return FReply::Handled();
	}
	
	return Result;
}

FReply SOutlinerView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const TArray<TWeakViewModelPtr<IOutlinerExtension>>& ItemsSourceRef = (*this->ItemsSource);

	auto IsSelectable = [](const TWeakViewModelPtr<IOutlinerExtension>& WeakModel)
	{
		TSharedPtr<ISelectableExtension> Selectable = WeakModel.ImplicitPin();
		return !Selectable || EnumHasAnyFlags(Selectable->IsSelectable(), ESelectionIntent::PersistentSelection);
	};

	// Don't respond to key-presses containing "Alt" as a modifier
	if (ItemsSourceRef.Num() > 0 && !InKeyEvent.IsAltDown())
	{
		bool bWasHandled = false;
		NullableItemType ItemNavigatedTo(nullptr);

		// Check for selection manipulation keys (Up, Down, Home, End, PageUp, PageDown)
		if (InKeyEvent.GetKey() == EKeys::Up)
		{
			int32 SelectionIndex = 0;
			if (TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::IsPtrValid(SelectorItem))
			{
				SelectionIndex = ItemsSourceRef.Find(TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::NullableItemTypeConvertToItemType(SelectorItem));
			}

			--SelectionIndex;

			for (; SelectionIndex >=0; --SelectionIndex)
			{
				if (IsSelectable(ItemsSourceRef[SelectionIndex]))
				{
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex];
					break;
				}
			}
			bWasHandled = true;
		}
		else if (InKeyEvent.GetKey() == EKeys::Down)
		{
			int32 SelectionIndex = 0;
			if (TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::IsPtrValid(SelectorItem))
			{
				SelectionIndex = ItemsSourceRef.Find(TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::NullableItemTypeConvertToItemType(SelectorItem));
			}

			++SelectionIndex;

			for (; SelectionIndex < ItemsSourceRef.Num(); ++SelectionIndex)
			{
				if (IsSelectable(ItemsSourceRef[SelectionIndex]))
				{
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex];
					break;
				}
			}
			bWasHandled = true;
		}
		else if (InKeyEvent.GetKey() == EKeys::Home)
		{
			// Select the first item
			for (int32 SelectionIndex = 0; SelectionIndex < ItemsSourceRef.Num(); ++SelectionIndex)
			{
				if (IsSelectable(ItemsSourceRef[SelectionIndex]))
				{
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex];
					break;
				}
			}
			bWasHandled = true;
		}
		else if (InKeyEvent.GetKey() == EKeys::End)
		{
			// Select the last item
			for (int32 SelectionIndex = ItemsSourceRef.Num() -1; SelectionIndex >=0 ; --SelectionIndex)
			{
				if (IsSelectable(ItemsSourceRef[SelectionIndex]))
				{
					ItemNavigatedTo = ItemsSourceRef[SelectionIndex];
					break;
				}
			}
			bWasHandled = true;
		}
		else if (InKeyEvent.GetKey() == EKeys::PageUp)
		{
			int32 SelectionIndex = 0;
			if (TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::IsPtrValid(SelectorItem))
			{
				SelectionIndex = ItemsSourceRef.Find(TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::NullableItemTypeConvertToItemType(SelectorItem));
			}

			int32 NumItemsInAPage = GetNumLiveWidgets();
			int32 Remainder = NumItemsInAPage % GetNumItemsPerLine();
			NumItemsInAPage -= Remainder;

			if (SelectionIndex >= NumItemsInAPage)
			{
				// Select an item on the previous page
				SelectionIndex = SelectionIndex - NumItemsInAPage;

				// Scan up for the first selectable node
				for (; SelectionIndex >= 0; --SelectionIndex)
				{
					if (IsSelectable(ItemsSourceRef[SelectionIndex]))
					{
						ItemNavigatedTo = ItemsSourceRef[SelectionIndex];
						break;
					}
				}
			}

			// If we had less than a page to jump, or we haven't found a selectable node yet,
			// scan back toward our current node until we find one.
			if (!ItemNavigatedTo.Pin())
			{
				SelectionIndex = 0;
				for (; SelectionIndex < ItemsSourceRef.Num(); ++SelectionIndex)
				{
					if (IsSelectable(ItemsSourceRef[SelectionIndex]))
					{
						ItemNavigatedTo = ItemsSourceRef[SelectionIndex];
						break;
					}
				}
			}

			bWasHandled = true;
		}
		else if (InKeyEvent.GetKey() == EKeys::PageDown)
		{
			int32 SelectionIndex = 0;
			if (TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::IsPtrValid(SelectorItem))
			{
				SelectionIndex = ItemsSourceRef.Find(TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::NullableItemTypeConvertToItemType(SelectorItem));
			}

			int32 NumItemsInAPage = GetNumLiveWidgets();
			int32 Remainder = NumItemsInAPage % GetNumItemsPerLine();
			NumItemsInAPage -= Remainder;


			if (SelectionIndex < ItemsSourceRef.Num() - NumItemsInAPage)
			{
				// Select an item on the next page
				SelectionIndex = SelectionIndex + NumItemsInAPage;

				for (; SelectionIndex < ItemsSourceRef.Num(); ++SelectionIndex)
				{
					if (IsSelectable(ItemsSourceRef[SelectionIndex]))
					{
						ItemNavigatedTo = ItemsSourceRef[SelectionIndex];
						break;
					}
				}
			}

			// If we had less than a page to jump, or we haven't found a selectable node yet,
			// scan back toward our current node until we find one.
			if (!ItemNavigatedTo.Pin())
			{
				SelectionIndex = ItemsSourceRef.Num() - 1;
				for (; SelectionIndex >= 0; --SelectionIndex)
				{
					if (IsSelectable(ItemsSourceRef[SelectionIndex]))
					{
						ItemNavigatedTo = ItemsSourceRef[SelectionIndex];
						break;
					}
				}
			}
			bWasHandled = true;
		}
		else if (InKeyEvent.GetKey() == EKeys::SpaceBar)
		{
			// SListView behavior is: Change selected status of item. Bypass that here. We don't want that, but instead want spacebar to go to SequencerCommands (toggle play)
			return FReply::Unhandled();
		}

		if (TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::IsPtrValid(ItemNavigatedTo))
		{
			TWeakViewModelPtr<IOutlinerExtension> ItemToSelect(TListTypeTraits<TWeakViewModelPtr<IOutlinerExtension>>::NullableItemTypeConvertToItemType(ItemNavigatedTo));
			NavigationSelect(ItemToSelect, InKeyEvent);
		}

		if (bWasHandled)
		{
			return FReply::Handled();
		}
	}

	return STreeView<TWeakViewModelPtr<IOutlinerExtension>>::OnKeyDown(MyGeometry, InKeyEvent);
}

void SOutlinerView::Private_SetItemSelection( TWeakViewModelPtr<IOutlinerExtension> TheItem, bool bShouldBeSelected, bool bWasUserDirected )
{
	if (TSharedPtr<FOutlinerSpacer> Spacer = TheItem.ImplicitPin())
	{
		return;
	}

	STreeView::Private_SetItemSelection( TheItem, bShouldBeSelected, bWasUserDirected );
}

void SOutlinerView::Private_ClearSelection()
{
	STreeView::Private_ClearSelection();
}

void SOutlinerView::Private_SelectRangeFromCurrentTo( TWeakViewModelPtr<IOutlinerExtension> InRangeSelectionEnd )
{
	STreeView::Private_SelectRangeFromCurrentTo(InRangeSelectionEnd);
}

void SOutlinerView::Private_SignalSelectionChanged(ESelectInfo::Type SelectInfo)
{
	// If bForwardToSelectionHandler is set here that means we are responding to a
	// selection change event that originated from a user interaction with this outliner view.
	// Therefore we must go through and re-assign selection states to the selection handler.

	STreeView::Private_SignalSelectionChanged(SelectInfo);

	TSharedPtr<IOutlinerSelectionHandler> SelectionHandler = SelectionHandlerAttribute.Get();
	if (SelectionHandler && bForwardToSelectionHandler)
	{
		TGuardValue<bool> Guard(bForwardToSelectionHandler, false);
		SelectionHandler->SelectOutlinerItems(GetSelectedItems(), bRightMouseButtonDown);
	}
}

TSharedPtr<SWidget> SOutlinerView::OnContextMenuOpening()
{
	TSharedPtr<SWidget> ContextMenu;

	// Open a context menu for the first selected item if it is selectable
	for (TWeakViewModelPtr<IOutlinerExtension> WeakSelectedItem : GetSelectedItems())
	{
		TViewModelPtr<IOutlinerExtension> SelectedItem = WeakSelectedItem.Pin();

		// Items are selectable by default, but can opt-in to conditionally selectable
		// TODO: This should be checked in the selection handler?
		TViewModelPtr<ISelectableExtension> Selectable = WeakSelectedItem.ImplicitPin();
		const bool bIsSelectable = !Selectable || EnumHasAnyFlags(Selectable->IsSelectable(), ESelectionIntent::ContextMenu);

		if (bIsSelectable)
		{
			TSharedPtr<FEditorViewModel> EditorViewModel = GetOutlinerModel()->GetEditor();
			ContextMenu = SelectedItem->CreateContextMenuWidget(EditorViewModel);
		}
		break;
	}

	// Otherwise, add a general menu for options
	TSharedPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin();
	if (Outliner && !ContextMenu)
	{
		ContextMenu = Outliner->CreateContextMenuWidget();
	}

	return ContextMenu;
}

void SOutlinerView::Refresh()
{
	TSharedPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin();

	RootNodes.Reset();
	if (!Outliner)
	{
		return;
	}

	for (TWeakViewModelPtr<IOutlinerExtension> WeakItem : Outliner->GetTopLevelItems())
	{
		TViewModelPtr<IOutlinerExtension> Extension = WeakItem.Pin();
		if (!Extension || Extension->IsFilteredOut())
		{
			continue;
		}

		// Only add pinned nodes if this is showing pinned only
		bool bIsPinned = false;

		constexpr bool bIncludeThis = true;
		for (const TViewModelPtr<IPinnableExtension>& Pinnable : Extension.AsModel()->GetAncestorsOfType<IPinnableExtension>(bIncludeThis))
		{
			if (Pinnable->IsPinned())
			{
				bIsPinned = true;
				break;
			}
		}

		if (bIsPinned == bShowPinnedNodes)
		{
			RootNodes.Add(Extension.AsModel());
		}
	}

	// Reset item expansion since we don't know if any expansion states may have changed in-between refreshes
	{
		STreeView::OnExpansionChanged.Unbind();

		ClearExpandedItems();

		FViewModelPtr RootItem = Outliner->GetRootItem();
		if (RootItem)
		{
			for (TViewModelPtr<IOutlinerExtension> Child : RootItem->GetDescendantsOfType<IOutlinerExtension>())
			{
				SetItemExpansion(Child, Child->IsExpanded());
			}
		}

		STreeView::OnExpansionChanged.BindSP(this, &SOutlinerView::OnExpansionChanged);
	}

	RebuildList();
	//RequestTreeRefresh();

	for (TSharedPtr<SOutlinerView> PinnedTreeView : PinnedTreeViews)
	{
		PinnedTreeView->Refresh();
	}
}

void SOutlinerView::ScrollByDelta(float DeltaInSlateUnits)
{
	ScrollBy( GetCachedGeometry(), DeltaInSlateUnits, EAllowOverscroll::No );
}

float SOutlinerView::GetVirtualTop() const
{
	return VirtualTop;
}

void SOutlinerView::HandleTableViewScrolled(double InScrollOffset)
{
	if (IsPendingRefresh())
	{
		bRefreshPhysicalGeometry = true;
	}
	else
	{
		UpdatePhysicalGeometry(false);
	}
}

void SOutlinerView::UpdatePhysicalGeometry(bool bIsRefresh)
{
	if (bIsRefresh)
	{
		// We need to first update virtual geometry in case the model hierarchy has changed, which is
		// very likely if we end up here from a tree view refresh.
		if (TSharedPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin())
		{
			if (FViewModelPtr RootItem = Outliner->GetRootItem())
			{
				IGeometryExtension::UpdateVirtualGeometry(0.f, RootItem);
			}
		}
	}

	const FChildren* Children = GetConstructedTableItems();
	const int32 NumSlots = Children->Num();

	PhysicalNodes.Empty();

	if (NumSlots == 0)
	{
		return;
	}

	const float FirstLineScrollOffset = GetFirstLineScrollOffset();
	float PhysicalOffset = 0.f;

	// Handle the first row: it might own a track lane, or reference a track lane owned by a parent
	// outliner item. In order to correctly position this track lane vertically, we need to accumulate
	// the item heights from whatever parents we have between the first row and the parent owns our
	// track lane.
	{
		TSharedRef<SOutlinerViewRow> FirstTreeViewRow  = StaticCastSharedRef<SOutlinerViewRow>(ConstCastSharedRef<SWidget>(Children->GetChildAt(0)));

		PhysicalOffset = -(FirstLineScrollOffset * FirstTreeViewRow->GetDesiredSize().Y);

		if (TSharedPtr<STrackLane> FirstRowTrackLane = FirstTreeViewRow->GetTrackLane(false))
		{
			FirstRowTrackLane->PositionParentTrackLanes(FirstTreeViewRow->GetDataModel(), PhysicalOffset);
		}
	}

	// Now we position any track lanes owned by the outliner items that are visible given the current scrolling.
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		TSharedRef<const SOutlinerViewRow> TreeViewRow = StaticCastSharedRef<const SOutlinerViewRow>(Children->GetChildAt(Index));

		// Only get owned track lanes (no parent references... we don't want to move parent lanes down
		// to children items)
		if (TSharedPtr<STrackLane> TrackLane = TreeViewRow->GetTrackLane(true))
		{
			TrackLane->SetVerticalPosition(PhysicalOffset);
		}

		TViewModelPtr<IOutlinerExtension> DataModel = TreeViewRow->GetDataModel();
		if (DataModel)
		{
			TSharedPtr<IGeometryExtension> GeometryExtension = DataModel.ImplicitCast();

			const FVector2D TreeViewRowSize = TreeViewRow->GetDesiredSize();
			const float TreeViewRowHeight = TreeViewRowSize.Y > 0 ? TreeViewRowSize.Y : TreeViewRow->GetDesiredSize().Y;
			const FVirtualGeometry VirtualGeometry = GeometryExtension ? GeometryExtension->GetVirtualGeometry() : FVirtualGeometry();

			PhysicalNodes.Add(FCachedGeometry{
				DataModel,
				PhysicalOffset,
				TreeViewRowHeight,
				VirtualGeometry.GetTop(),
				VirtualGeometry.GetHeight(),
				VirtualGeometry.GetNestedHeight()
			});
		}

		PhysicalOffset += TreeViewRow->GetDesiredSize().Y;
	}
}

bool ShouldExpand(const TArray<TWeakViewModelPtr<IOutlinerExtension>>& Selection, ETreeRecursion Recursion, TSharedPtr<FOutlinerViewModel> Outliner)
{
	bool bAllExpanded = true;
	for (TWeakViewModelPtr<IOutlinerExtension> WeakItem : Selection)
	{
		// @todo_sequencer_mvvm: Do we need to check selection state here if it's already in a supposed selection set??
		TViewModelPtr<IOutlinerExtension> SelectedItem = WeakItem.Pin();
		if (!SelectedItem || !SelectedItem->IsExpanded())
		{
			return true;
		}

		if (Recursion == ETreeRecursion::Recursive)
		{
			for (const TViewModelPtr<IOutlinerExtension>& Child : SelectedItem.AsModel()->GetChildrenOfType<IOutlinerExtension>())
			{
				if (!Child->IsExpanded())
				{
					return true;
				}
			}
		}
	}
	return false;
}

void SOutlinerView::ToggleExpandCollapseNodes(ETreeRecursion Recursion, bool bExpandAll, bool bCollapseAll)
{
	TSharedPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin();
	if (!Outliner)
	{
		return;
	}

	TArray<TWeakViewModelPtr<IOutlinerExtension>> Selection = GetSelectedItems();

	if (Selection.Num() > 0 && !bExpandAll && !bCollapseAll)
	{
		const bool bExpand = ShouldExpand(Selection, Recursion, Outliner);
		
		for (TWeakViewModelPtr<IOutlinerExtension> WeakItem : Selection)
		{
			TViewModelPtr<IOutlinerExtension> Item = WeakItem.Pin();
			if (Item)
			{
				ExpandCollapseNode(Item, bExpand, Recursion);
			}
		}
	}
	else if (ItemsSource->Num() > 0)
	{
		bool bExpand = ShouldExpand(*ItemsSource, Recursion, Outliner);

		if (bExpandAll)
		{
			bExpand = true;
		}
		if (bCollapseAll)
		{
			bExpand = false;
		}

		for (TWeakViewModelPtr<IOutlinerExtension> WeakItem : *ItemsSource)
		{
			if (TViewModelPtr<IOutlinerExtension> Item = WeakItem.Pin())
			{
				ExpandCollapseNode(Item, bExpand, Recursion);
			}
		}
	}
}

void SOutlinerView::ExpandCollapseNode(TViewModelPtr<IOutlinerExtension> InViewModel, bool bExpansionState, ETreeRecursion Recursion)
{
	SetItemExpansion(InViewModel, bExpansionState);

	if (Recursion == ETreeRecursion::Recursive)
	{
		for (TViewModelPtr<IOutlinerExtension> Child : InViewModel.AsModel()->GetChildrenOfType<IOutlinerExtension>())
		{
			ExpandCollapseNode(Child, bExpansionState, ETreeRecursion::Recursive);
		}
	}
}

void SOutlinerView::OnExpansionChanged(TWeakViewModelPtr<IOutlinerExtension> InWeakItem, bool bIsExpanded)
{
	TViewModelPtr<IOutlinerExtension> SelectedItem = InWeakItem.Pin();
	if (!SelectedItem)
	{
		return;
	}

	SelectedItem->SetExpansion(bIsExpanded);

	// Expand any children that are also expanded
	for (const TViewModelPtr<IOutlinerExtension>& Child : SelectedItem.AsModel()->GetChildrenOfType<IOutlinerExtension>())
	{
		if (Child->IsExpanded())
		{
			SetItemExpansion(Child, true);
		}
	}
}

void SOutlinerView::SetItemExpansionRecursive(TWeakViewModelPtr<IOutlinerExtension> InItem, bool bIsExpanded)
{
	if (TViewModelPtr<IOutlinerExtension> Item = InItem.Pin())
	{
		ExpandCollapseNode(Item, bIsExpanded, ETreeRecursion::Recursive);
	}
}

void SOutlinerView::OnGetChildren(TWeakViewModelPtr<IOutlinerExtension> InParent, TArray<TWeakViewModelPtr<IOutlinerExtension>>& OutChildren) const
{
	TSharedPtr<FOutlinerViewModel> Outliner  = WeakOutliner.Pin();
	TViewModelPtr<IOutlinerExtension> DataModel = InParent.Pin();

	if (Outliner && DataModel)
	{
		for (TViewModelPtr<IOutlinerExtension> Child : DataModel.AsModel()->GetChildrenOfType<IOutlinerExtension>())
		{
			if (!Child->IsFilteredOut())
			{
				OutChildren.Add(Child);
			}
		}
	}
}

TSharedPtr<STrackLane> SOutlinerView::FindOrCreateParentLane(TViewModelPtr<IOutlinerExtension> InDataModel)
{
	TSharedPtr<FOutlinerViewModel> Outliner  = WeakOutliner.Pin();
	if (!Outliner)
	{
		return nullptr;
	}

	// Find any parent that will create a nested track lane
	for (const TViewModelPtr<ITrackAreaExtension>& ParentTrackArea : InDataModel.AsModel()->GetAncestorsOfType<ITrackAreaExtension>())
	{
		FTrackAreaParameters TrackAreaParameters = ParentTrackArea->GetTrackAreaParameters();
		if (TrackAreaParameters.LaneType == ETrackAreaLaneType::Nested)
		{
			// Find the first outliner extension parent (should be the same as ParentTrackArea really)
			for (const TViewModelPtr<IOutlinerExtension>& OutlinerExtItem : ParentTrackArea.AsModel()->GetAncestorsOfType<IOutlinerExtension>(true))
			{
				TSharedPtr<STrackLane> TrackLane = TrackArea->FindTrackSlot(OutlinerExtItem);
				if (!TrackLane)
				{
					// Add a track slot for the nested track lane parent if it doesn't already exist
					TrackLane = SNew(STrackLane, TrackArea, OutlinerExtItem, nullptr, TrackAreaParameters, SharedThis(this));
					TrackArea->AddTrackSlot(OutlinerExtItem, TrackLane);
				}

				return TrackLane;
			}

			// Once we find any track area extension that wants nested lanes, return if it didn't manage to create one
			break;
		}
	}

	return nullptr;
}

void SOutlinerView::CreateTrackLanesForRow(TSharedRef<SOutlinerViewRow> InRow, TViewModelPtr<IOutlinerExtension> InDataModel)
{
	// Create a lane for this item, if needed, or find a lane to use from one of its parents.
	// We also create a lane on the appropriate parent if it doesn't exist, because we could be in a situation
	// where we jump-scrolled directly to the given row and item without ever having had the parent in view.
	const bool bIncludeDataModel = true;
	for (TViewModelPtr<IOutlinerExtension> CurDataModel : InDataModel.AsModel()->GetAncestors(bIncludeDataModel))
	{
		if (TSharedPtr<ITrackAreaExtension> TrackAreaExtension = CurDataModel.ImplicitCast())
		{
			FTrackAreaParameters Parameters = TrackAreaExtension->GetTrackAreaParameters();
			if (Parameters.LaneType != ETrackAreaLaneType::None)
			{
				TSharedPtr<STrackLane> TrackLane = TrackArea->FindTrackSlot(CurDataModel);
				if (!TrackLane)
				{
					// Add a track slot for the row
					TSharedPtr<STrackLane> ParentLane = FindOrCreateParentLane(CurDataModel);
					TrackLane = SNew(STrackLane, TrackArea, CurDataModel, ParentLane, Parameters, SharedThis(this));

					TrackArea->AddTrackSlot(CurDataModel, TrackLane);
				}
				ensure(TrackLane);
				InRow->SetTrackLane(TrackLane);
				break;
			}
		}
	}
}

TSharedRef<ITableRow> SOutlinerView::OnGenerateRow(TWeakViewModelPtr<IOutlinerExtension> InWeakModel, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SOutlinerViewRow> Row =
		SNew(SOutlinerViewRow, OwnerTable, InWeakModel)
		.OnDetectDrag(this, &SOutlinerView::OnDragRow)
		.OnGenerateWidgetForColumn(this, &SOutlinerView::GenerateWidgetForColumn);

	if (TViewModelPtr<IOutlinerExtension> ViewModel = InWeakModel.Pin())
	{
		CreateTrackLanesForRow(Row, ViewModel);
	}
	return Row;
}

TSharedRef<SWidget> SOutlinerView::GenerateWidgetForColumn(TViewModelPtr<IOutlinerExtension> InDataModel, const FName& ColumnId, const TSharedRef<SOutlinerViewRow>& Row) const
{
	const FOutlinerViewColumn* Definition = Columns.Find(ColumnId);

	if (ensureMsgf(Definition, TEXT("Invalid column name specified")))
	{
		return Definition->Generator(InDataModel, Row);
	}

	return SNullWidget::NullWidget;
}

} // namespace Sequencer
} // namespace UE

