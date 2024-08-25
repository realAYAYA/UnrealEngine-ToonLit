// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Layers/SDMSlotLayerView.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialStage.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorCommands.h"
#include "DynamicMaterialEditorSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Menus/DMMaterialSlotLayerMenus.h"
#include "ScopedTransaction.h"
#include "SDMLayerEffectsItem.h"
#include "Slate/Layers/SDMSlotLayerItem.h"
#include "Slate/SDMSlot.h"
#include "ToolMenus.h"
#include "Components/DMMaterialEffect.h"
#include "Components/DMMaterialEffectStack.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SDMSlotLayerView"

void SDMSlotLayerView::Construct(const FArguments& InArgs, const TSharedRef<SDMSlot>& InSlotWidget)
{
	SlotWidgetWeak = InSlotWidget;
	MaterialSlotWeak = InSlotWidget->GetSlot();
	ensure(MaterialSlotWeak.IsValid());

	PreviewSize = InArgs._PreviewSize;
	LayerSelected = InArgs._OnLayerSelected;
	LayerStageSelected = InArgs._OnLayerStageSelected;

	bPostRegenSelect = false;

	SListView::Construct(SListView::FArguments()
		.ListItemsSource(&LayerItems)
		.SelectionMode(ESelectionMode::Single)
		.ItemHeight_Lambda([]() { return UDynamicMaterialEditorSettings::Get()->LayerPreviewSize + 32.0f; })
		.ClearSelectionOnClick(false)
		.EnableAnimatedScrolling(true)
		.ScrollbarVisibility(EVisibility::Visible)
		.ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
		.OnGenerateRow(this, &SDMSlotLayerView::OnGenerateLayerItemWidget)
		.OnSelectionChanged(this, &SDMSlotLayerView::OnLayerItemSelectionChanged)
		.OnContextMenuOpening(this, &SDMSlotLayerView::CreateLayerItemContextMenu)
	);

	RegenerateItems();
	RequestListRefresh();
	BindCommands();	
}

TSharedRef<ITableRow> SDMSlotLayerView::OnGenerateLayerItemWidget(TSharedPtr<FDMMaterialLayerReference> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	TAttribute<bool> StageBaseEnabledAttr;
	TAttribute<bool> StageBaseSelectedAttr;
	TAttribute<bool> StageMaskEnabledAttr;
	TAttribute<bool> StageMaskSelectedAttr;

	if (ensure(InItem.IsValid()))
	{
		StageBaseEnabledAttr.Bind(InItem.ToSharedRef(), &FDMMaterialLayerReference::IsBaseEnabled);
		StageBaseSelectedAttr.Bind(InItem.ToSharedRef(), &FDMMaterialLayerReference::IsBaseBeingEdited);
		StageMaskEnabledAttr.Bind(InItem.ToSharedRef(), &FDMMaterialLayerReference::IsMaskEnabled);
		StageMaskSelectedAttr.Bind(InItem.ToSharedRef(), &FDMMaterialLayerReference::IsMaskBeingEdited);
	}

	TSharedRef<SDMSlotLayerItem> LayerItem = SNew(SDMSlotLayerItem, SlotWidgetWeak.Pin(), InOwnerTable, InItem)
		.PreviewSize(PreviewSize)
		.OnStageSelected(this, &SDMSlotLayerView::OnLayerStageSelected)
		.OnLayerLinkToggled(this, &SDMSlotLayerView::OnLayerLinkToggled)
		.StageBaseEnabled(StageBaseEnabledAttr)
		.StageBaseSelected(StageBaseSelectedAttr)
		.StageMaskEnabled(StageMaskEnabledAttr)
		.StageMaskSelected(StageMaskSelectedAttr);

	if (UDMMaterialLayerObject* Layer = InItem->GetLayer())
	{
		const int32 LayerIndex = Layer->FindIndex();

		if (LayerIndex != INDEX_NONE)
		{
			if (!StageWidgets.IsValidIndex(LayerIndex))
			{
				StageWidgets.SetNum(LayerItems.Num());
			}

			if (StageWidgets.IsValidIndex(LayerIndex))
			{
				StageWidgets[LayerIndex].Add(EDMMaterialLayerStage::Base, LayerItem->GetBaseStageWidget());
				StageWidgets[LayerIndex].Add(EDMMaterialLayerStage::Mask, LayerItem->GetMaskStageWidget());
			}
		}
	}

	return LayerItem;
}

void SDMSlotLayerView::Internal_SelectLayers(const TArray<TSharedPtr<FDMMaterialLayerReference>>& InSelectedLayers, UDMMaterialStage* InStage)
{
	bool bHasChangedBeingEdited = false;

	auto EnableSelectedStages = [this, &bHasChangedBeingEdited](const bool bInBeingEdited)
	{
		for (const TWeakObjectPtr<UDMMaterialStage>& SelectedStageWeak : SelectedStages)
		{
			if (UDMMaterialStage* SelectedStage = SelectedStageWeak.Get())
			{
				bHasChangedBeingEdited &= SelectedStage->SetBeingEdited(bInBeingEdited);
			}
		}
	};

	// Disable editing for all previously selected stages.
	EnableSelectedStages(false);

	// @TODO: Multiple stage selection is currently not fully supported.
	SelectedStages = { InStage };

	// Enable editing for all new selected stages.
	EnableSelectedStages(true);

	// Find the selected layer index.
	SelectedLayerIndex = INDEX_NONE;

	if (InSelectedLayers.Num() == 1)
	{
		for (int32 Index = 0; Index < LayerItems.Num(); ++Index)
		{
			if (LayerItems[Index] == InSelectedLayers[0])
			{
				SelectedLayerIndex = Index;
			}
		}
	}

	if (bHasChangedBeingEdited)
	{
		if (TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin())
		{
			SlotWidget->InvalidateSlotSettingsRowWidget();
		}
	}
}

void SDMSlotLayerView::RegenerateItems()
{
	LayerItems.Empty();

	UDMMaterialSlot* Slot = MaterialSlotWeak.Get();

	if (!Slot)
	{
		return;
	}

	const TArray<TObjectPtr<UDMMaterialLayerObject>>& SlotLayers = Slot->GetLayers();
	const int LayerCount = SlotLayers.Num();

	LayerItems.Reserve(LayerCount);

	for (int32 LayerIdx = LayerCount - 1; LayerIdx >= 0; --LayerIdx)
	{
		LayerItems.Add(MakeShared<FDMMaterialLayerReference>(SlotLayers[LayerIdx]));
	}
}

void SDMSlotLayerView::BindCommands()
{
	TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin();

	if (!SlotWidget.IsValid())
	{
		return;
	}

	const FDynamicMaterialEditorCommands& Commands = FDynamicMaterialEditorCommands::Get();

	SlotWidget->GetEditorWidget()->GetCommandList()->MapAction(
		Commands.SelectLayerBaseStage,
		FExecuteAction::CreateSP(this, &SDMSlotLayerView::ExecuteSelectLayerStage, EDMMaterialLayerStage::Base),
		FCanExecuteAction::CreateSP(this, &SDMSlotLayerView::CanSelectLayerStage, EDMMaterialLayerStage::Base)
	);

	SlotWidget->GetEditorWidget()->GetCommandList()->MapAction(
		Commands.SelectLayerMaskStage,
		FExecuteAction::CreateSP(this, &SDMSlotLayerView::ExecuteSelectLayerStage, EDMMaterialLayerStage::Mask),
		FCanExecuteAction::CreateSP(this, &SDMSlotLayerView::CanSelectLayerStage, EDMMaterialLayerStage::Mask)
	);

	SlotWidget->GetEditorWidget()->GetCommandList()->MapAction(
		Commands.MoveLayerUp,
		FExecuteAction::CreateSP(this, &SDMSlotLayerView::ExecuteMoveLayer, -1),
		FCanExecuteAction::CreateSP(this, &SDMSlotLayerView::CanMoveLayer, -1)
	);

	SlotWidget->GetEditorWidget()->GetCommandList()->MapAction(
		Commands.MoveLayerDown,
		FExecuteAction::CreateSP(this, &SDMSlotLayerView::ExecuteMoveLayer, 1),
		FCanExecuteAction::CreateSP(this, &SDMSlotLayerView::CanMoveLayer, 1)
	);
}

bool SDMSlotLayerView::CanMoveLayer(int32 InOffset) const
{
	if (SelectedLayerIndex == INDEX_NONE)
	{
		return false;
	}

	// Already validated by CanMoveLayer
	UDMMaterialSlot* Slot = MaterialSlotWeak.Get();

	const TArray<TObjectPtr<UDMMaterialLayerObject>>& SlotLayers = Slot->GetLayers();

	const int32 LayerCount = SlotLayers.Num();
	const int32 SelectedSlotLayerIndex = LayerCount - SelectedLayerIndex - 1;

	if (UDMMaterialLayerObject* Layer = Slot->GetLayer(SelectedSlotLayerIndex))
	{
		return !!Layer->GetFirstValidStage(EDMMaterialLayerStage::All);
	}

	return false;
}

void SDMSlotLayerView::ExecuteMoveLayer(int32 InOffset)
{
	if (!CanMoveLayer(InOffset))
	{
		return;
	}

	const int32 NewIndex = SelectedLayerIndex + InOffset;

	// Already validated by CanMoveLayer
	UDMMaterialSlot* Slot = MaterialSlotWeak.Get();

	const TArray<TObjectPtr<UDMMaterialLayerObject>>& SlotLayers = Slot->GetLayers();

	const int32 LayerCount = SlotLayers.Num();
	const int32 SelectedSlotLayerIndex = LayerCount - SelectedLayerIndex - 1;
	const int32 NewSlotIndex = LayerCount - NewIndex - 1;

	if (UDMMaterialLayerObject* Layer = Slot->GetLayer(SelectedSlotLayerIndex))
	{
		FScopedTransaction Transaction(LOCTEXT("MoveLayer", "Material Designer Move Layer"));
		Slot->Modify();

		if (Slot->MoveLayer(Layer, NewSlotIndex))
		{
			PostRegenSelectedStages = SelectedStages;
			PostRegenSelectedLayerIndex = NewSlotIndex;
			bPostRegenSelect = true;
			RequestListRefresh();
		}
	}
}

bool SDMSlotLayerView::CanSelectLayerStage(EDMMaterialLayerStage InStageType) const
{
	if (SelectedLayerIndex == INDEX_NONE
		|| !LayerItems.IsValidIndex(SelectedLayerIndex) 
		|| !MaterialSlotWeak.IsValid())
	{
		return false;
	}

	UDMMaterialSlot* Slot = MaterialSlotWeak.Get();

	const TArray<TObjectPtr<UDMMaterialLayerObject>>& SlotLayers = Slot->GetLayers();

	const int32 LayerCount = SlotLayers.Num();
	const int32 SelectedSlotLayerIndex = LayerCount - SelectedLayerIndex - 1;

	if (const TObjectPtr<UDMMaterialLayerObject>& Layer = Slot->GetLayer(SelectedSlotLayerIndex))
	{
		if (StageWidgets.IsValidIndex(SelectedLayerIndex))
		{
			if (const TWeakPtr<SDMStage>* StageWidgetWeakPtr = StageWidgets[SelectedLayerIndex].Find(InStageType))
			{
				if (StageWidgetWeakPtr->IsValid())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void SDMSlotLayerView::ExecuteSelectLayerStage(EDMMaterialLayerStage InStageType)
{
	if (!CanSelectLayerStage(InStageType))
	{
		return;
	}

	// Already validated by CanMoveLayer
	UDMMaterialSlot* Slot = MaterialSlotWeak.Get();
	
	const TArray<TObjectPtr<UDMMaterialLayerObject>>& SlotLayers = Slot->GetLayers();

	const int32 LayerCount = SlotLayers.Num();
	const int32 SelectedSlotLayerIndex = LayerCount - SelectedLayerIndex - 1;

	if (UDMMaterialLayerObject* Layer = Slot->GetLayer(SelectedSlotLayerIndex))
	{
		if (StageWidgets.IsValidIndex(SelectedSlotLayerIndex))
		{
			if (const TWeakPtr<SDMStage>* StageWidgetWeakPtr = StageWidgets[SelectedSlotLayerIndex].Find(InStageType))
			{
				if (TSharedPtr<SDMStage> StageWidget = StageWidgetWeakPtr->Pin())
				{
					OnLayerStageSelected(true, StageWidget.ToSharedRef());
				}
			}
		}
	}
}

void SDMSlotLayerView::OnUndo()
{
	RequestListRefresh();
}

void SDMSlotLayerView::OnLayerItemSelectionChanged(TSharedPtr<FDMMaterialLayerReference> InSelectedItem, ESelectInfo::Type InSelectInfo)
{
	TSharedPtr<SDMSlotLayerItem> SlotLayerWidget = WidgetFromLayerItem(InSelectedItem);
	if (SlotLayerWidget.IsValid())
	{
		SlotLayerWidget->DeselectAllEffects();
	}

	Internal_SelectLayers({ InSelectedItem });

	TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin();

	FScopedTransaction Transaction(LOCTEXT("SelectLayer", "Material Designer Select Layer"));

	for (int32 LayerItemIdx = 0; LayerItemIdx < LayerItems.Num(); ++LayerItemIdx)
	{
		if (LayerItemIdx == SelectedLayerIndex)
		{
			continue;
		}

		TSharedPtr<ITableRow> RowWidget = WidgetFromItem(LayerItems[LayerItemIdx]);

		if (RowWidget.IsValid())
		{
			TSharedPtr<SDMSlotLayerItem> LayerWidget = StaticCastSharedPtr<SDMSlotLayerItem>(RowWidget);

			if (TSharedPtr<SDMStage> BaseWidget = LayerWidget->GetBaseStageWidget())
			{
				if (UDMMaterialStage* Stage = BaseWidget->GetStage())
				{
					Stage->Modify();
					Stage->SetBeingEdited(false);

					if (InSelectInfo == ESelectInfo::OnMouseClick && SlotWidget.IsValid())
					{
						SlotWidget->InvalidateSlotSettingsRowWidget();
						SlotWidget->InvalidateComponentEditWidget();
					}
				}
			}

			if (TSharedPtr<SDMStage> MaskWidget = LayerWidget->GetMaskStageWidget())
			{
				if (UDMMaterialStage* Stage = MaskWidget->GetStage())
				{
					Stage->SetBeingEdited(false);

					if (InSelectInfo == ESelectInfo::OnMouseClick && SlotWidget.IsValid())
					{
						SlotWidget->InvalidateSlotSettingsRowWidget();
						SlotWidget->InvalidateComponentEditWidget();
					}
				}
			}
		}
	}
}

void SDMSlotLayerView::OnLayerStageSelected(const bool bInSelected, const TSharedRef<SDMStage>& InStageWidget)
{
	UDMMaterialStage* SelectedStage = InStageWidget->GetStage();
	if (!SelectedStage)
	{
		return;
	}

	TSharedPtr<FDMMaterialLayerReference> LayerItem = FindLayerItem(SelectedStage);
	if (!LayerItem.IsValid())
	{
		return;
	}

	Internal_SelectLayers({ LayerItem }, SelectedStage);

	SelectedStageWidgets.Empty();
	SelectedStageWidgets.Add(InStageWidget);
	
	LayerStageSelected.ExecuteIfBound(bInSelected, InStageWidget);
}
 
void SDMSlotLayerView::OnLayerLinkToggled(const bool bIsLinkActive, const TSharedRef<SDMStage>& InStageWidget)
{
	if (UDMMaterialStage* Stage = InStageWidget->GetStage())
	{
		if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			if (UDMMaterialSlot* Slot = Layer->GetSlot())
			{
				if (TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin())
				{
					TArray<int32> SelectedLayers = SlotWidget->GetSelectedLayerIndices();

					if (SelectedLayers.IsEmpty() == false)
					{
						const TArray<TObjectPtr<UDMMaterialLayerObject>>& Layers = Slot->GetLayers();

						for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
						{
							if (Layers[LayerIdx]->HasValidStage(Stage))
							{
								if (LayerIdx == SelectedLayers[0])
								{
									SlotWidget->InvalidateComponentEditWidget();
								}
							}
						}
					}
				}
			}
		}
	}
}

TSharedPtr<SDMSlotLayerItem> SDMSlotLayerView::WidgetFromLayerItem(const TSharedPtr<FDMMaterialLayerReference>& InItem)
{
	return StaticCastSharedPtr<SDMSlotLayerItem>(WidgetFromItem(InItem));
}

TSharedPtr<SWidget> SDMSlotLayerView::CreateLayerItemContextMenu()
{
	UDMMaterialLayerObject* LayerObject = nullptr;
	FVector2f MousePosition = FSlateApplication::Get().GetCursorPos();

	for (const TSharedPtr<FDMMaterialLayerReference>& LayerItem : LayerItems)
	{
		TSharedPtr<SDMSlotLayerItem> LayerItemWidget = WidgetFromLayerItem(LayerItem);

		if (LayerItemWidget.IsValid() && LayerItemWidget->GetTickSpaceGeometry().IsUnderLocation(MousePosition))
		{
			LayerObject = LayerItem->GetLayer();
			break;
		}
	}
	
	if (TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin())
	{
		UToolMenu* ContextMenu = FDMMaterialSlotLayerMenus::GenerateSlotLayerMenu(SlotWidget, LayerObject);
		return UToolMenus::Get()->GenerateWidget(ContextMenu);
	}

	return SNullWidget::NullWidget;
}

void SDMSlotLayerView::ScrollItemIntoView(const TSharedPtr<FDMMaterialLayerReference>& InItem)
{
	bNavigateOnScrollIntoView = true;

	RequestScrollIntoView(InItem, 0);
}

void SDMSlotLayerView::FocusOnItem(const TSharedPtr<FDMMaterialLayerReference>& InItem)
{
	SelectorItem = InItem;
	RangeSelectionStart = InItem;
}

int32 SDMSlotLayerView::GetLayerItemIndex(const TSharedPtr<FDMMaterialLayerReference>& Item) const
{
	if (HasValidItemsSource())
	{
		return GetItems().Find(TListTypeTraits<TSharedPtr<FDMMaterialLayerReference>>::NullableItemTypeConvertToItemType(Item));
	}

	return INDEX_NONE;
}

FCursorReply SDMSlotLayerView::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (IsRightClickScrolling() && CursorEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		// We hide the native cursor as we'll be drawing the software EMouseCursor::GrabHandClosed cursor
		return FCursorReply::Cursor(EMouseCursor::None);
	}
	return SListView::OnCursorQuery(MyGeometry, CursorEvent);
}

FReply SDMSlotLayerView::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && !MouseEvent.IsTouchEvent())
	{
		// We only care about deltas along the scroll axis
		FTableViewDimensions CursorDeltaDimensions(Orientation, MouseEvent.GetCursorDelta());
		CursorDeltaDimensions.LineAxis = 0.f;

		const float ScrollByAmount = CursorDeltaDimensions.ScrollAxis / MyGeometry.Scale;

		// If scrolling with the right mouse button, we need to remember how much we scrolled.
		// If we did not scroll at all, we will bring up the context menu when the mouse is released.
		AmountScrolledWhileRightMouseDown += FMath::Abs(ScrollByAmount);

		// Has the mouse moved far enough with the right mouse button held down to start capturing
		// the mouse and dragging the view?
		if (IsRightClickScrolling())
		{
			// Make sure the active timer is registered to update the inertial scroll
			if (!bIsScrollingActiveTimerRegistered)
			{
				bIsScrollingActiveTimerRegistered = true;
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SDMSlotLayerView::UpdateInertialScroll));
			}

			TickScrollDelta -= ScrollByAmount;

			const float AmountScrolled = this->ScrollBy(MyGeometry, -ScrollByAmount, AllowOverscroll);

			FReply Reply = FReply::Handled();

			// The mouse moved enough that we're now dragging the view. Capture the mouse
			// so the user does not have to stay within the bounds of the list while dragging.
			if (this->HasMouseCapture() == false)
			{
				Reply.CaptureMouse(AsShared()).UseHighPrecisionMouseMovement(AsShared());
				SoftwareCursorPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				bShowSoftwareCursor    = true;
			}

			// Check if the mouse has moved.
			if (AmountScrolled != 0)
			{
				SoftwareCursorPosition += CursorDeltaDimensions.ToVector2D();
			}

			return Reply;
		}
	}

	return SListView::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SDMSlotLayerView::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FReply Reply = FReply::Handled().ReleaseMouseCapture();
		AmountScrolledWhileRightMouseDown = 0;
		bShowSoftwareCursor = false;

		// If we have mouse capture, snap the mouse back to the closest location that is within the list's bounds
		if (HasMouseCapture())
		{
			const FSlateRect ListScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();
			const FVector2D CursorPosition = MyGeometry.LocalToAbsolute(SoftwareCursorPosition);

			const FIntPoint BestPositionInList(
				FMath::RoundToInt(FMath::Clamp(CursorPosition.X, ListScreenSpaceRect.Left, ListScreenSpaceRect.Right)),
				FMath::RoundToInt(FMath::Clamp(CursorPosition.Y, ListScreenSpaceRect.Top, ListScreenSpaceRect.Bottom))
			);

			Reply.SetMousePos(BestPositionInList);
		}

		return Reply;
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bool bHovering = false;
		FVector2f MousePosition = FSlateApplication::Get().GetCursorPos();

		for (const TSharedPtr<FDMMaterialLayerReference>& LayerItem : LayerItems)
		{
			TSharedPtr<SDMSlotLayerItem> LayerItemWidget = WidgetFromLayerItem(LayerItem);

			if (LayerItemWidget.IsValid() && LayerItemWidget->GetTickSpaceGeometry().IsUnderLocation(MousePosition))
			{
				bHovering = true;
				break;
			}
		}
	}

	return SListView::OnMouseButtonUp(MyGeometry, MouseEvent);
}

void SDMSlotLayerView::RequestListRefresh()
{
	RegenerateItems();

	SListView<TSharedPtr<FDMMaterialLayerReference>>::RequestListRefresh();
}

void SDMSlotLayerView::PostUndo(bool bSuccess)
{
	OnUndo();
}

void SDMSlotLayerView::PostRedo(bool bSuccess)
{
	OnUndo();
}

TSharedPtr<FDMMaterialLayerReference> SDMSlotLayerView::FindLayerItem(UDMMaterialStage* const InStage) const
{
	if (!IsValid(InStage))
	{
		return nullptr;
	}

	for (const TSharedPtr<FDMMaterialLayerReference>& LayerItem : LayerItems)
	{
		if (LayerItem.IsValid())
		{
			if (UDMMaterialLayerObject* Layer = LayerItem->GetLayer())
			{
				if (Layer->HasValidStage(InStage))
				{
					return LayerItem;
				}
			}
		}
	}

	return nullptr;
}

TSharedPtr<FDMMaterialLayerReference> SDMSlotLayerView::FindLayerItem(UDMMaterialLayerObject* const InLayer) const
{
	if (!MaterialSlotWeak.IsValid())
	{
		return nullptr;
	}

	for (const TSharedPtr<FDMMaterialLayerReference>& LayerItem : LayerItems)
	{
		if (LayerItem.IsValid())
		{
			if (UDMMaterialLayerObject* Layer = LayerItem->GetLayer())
			{
				if (Layer == InLayer)
				{
					return LayerItem;
				}
			}
		}
	}

	return nullptr;
}

void SDMSlotLayerView::SelectLayerItem(UDMMaterialStage* const InStage, const bool bInSelected, const ESelectInfo::Type InSelectInfo)
{
	TSharedPtr<FDMMaterialLayerReference> Item = FindLayerItem(InStage);
	SetItemSelection(Item, bInSelected, InSelectInfo);
}

void SDMSlotLayerView::SelectLayerItem(UDMMaterialLayerObject* const InLayer, const bool bInMask, const bool bInSelected, const ESelectInfo::Type InSelectInfo)
{
	TSharedPtr<FDMMaterialLayerReference> Item = FindLayerItem(InLayer);
	SetItemSelection(Item, bInSelected, InSelectInfo);
}

bool SDMSlotLayerView::AddLayerItem(const TSharedPtr<FDMMaterialLayerReference>& InLayerItem)
{
	if (!InLayerItem.IsValid())
	{
		return false;
	}

	// The items themselves are only links to indices in an array. Regenerating the list will be quick
	// and more than enough to get an accurate display.

	RequestListRefresh();

	return true;
}

bool SDMSlotLayerView::RemoveLayerItem(const TSharedPtr<FDMMaterialLayerReference>& InLayerItem)
{
	if (!InLayerItem.IsValid())
	{
		return false;
	}

	// The items themselves are only links to indices in an array. Regenerating the list will be quick
	// and more than enough to get an accurate display.

	RequestListRefresh();

	return true;
}

void SDMSlotLayerView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SListView<TSharedPtr<FDMMaterialLayerReference, ESPMode::ThreadSafe>>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bPostRegenSelect && LayerItems.IsValidIndex(PostRegenSelectedLayerIndex))
	{
		FScopedTransaction Transaction(LOCTEXT("SelectLayer", "Material Designer Select Layer"));

		for (const TWeakObjectPtr<UDMMaterialStage>& SelectedStageWeak : PostRegenSelectedStages)
		{
			if (UDMMaterialStage* SelectedStage = SelectedStageWeak.Get())
			{
				SelectedStage->Modify();
				SelectedStage->SetBeingEdited(true);
			}
		}

		SelectedStages = PostRegenSelectedStages;
	}

	bPostRegenSelect = false;
	PostRegenSelectedLayerIndex = INDEX_NONE;
	PostRegenSelectedStages.Empty();
}

#undef LOCTEXT_NAMESPACE
